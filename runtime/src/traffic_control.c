#include <unistd.h>

#include "traffic_control.h"
#include "debuglog.h"
#include "global_request_scheduler_mtdbf.h"
#include "tenant_functions.h"
#include "sandbox_set_as_error.h"
#include "dbf.h"

#ifdef TRAFFIC_CONTROL
// void *global_dbf;
void **global_virt_worker_dbfs;
void *global_worker_dbf; // temp ///////////

extern struct priority_queue *global_request_scheduler_mtdbf;//, *global_default;
extern lock_t global_lock;

void
traffic_control_initialize()
{
	assert(runtime_max_deadline > 0);

	const int N_VIRT_WORKERS_DBF = USING_AGGREGATED_GLOBAL_DBF ? 1 : runtime_worker_threads_count;
	global_virt_worker_dbfs = malloc(N_VIRT_WORKERS_DBF * sizeof(void*));
	for (int i = 0; i < N_VIRT_WORKERS_DBF; i++) {
		global_virt_worker_dbfs[i] = dbf_initialize(runtime_worker_threads_count/N_VIRT_WORKERS_DBF, 100, -1, NULL);
	}

}

void
traffic_control_log_decision(const int num, const bool admitted)
{
#ifdef LOG_TRAFFIC_CONTROL
	debuglog("Admission case #: %d, Admitted? %s\n", num, admitted ? "yes" : "no");
#endif /* LOG_TRAFFIC_CONTROL */
}
int ind = 0;
int
global_virt_worker_dbfs_try_update_demand(uint64_t start_time, uint64_t adjustment, uint64_t *time_oversupply_p, struct sandbox_metadata *sm)
{
	bool global_can_admit = false;
	uint64_t time_oversupply = 0;
	const uint64_t absolute_deadline = sm->absolute_deadline;
	const int N_VIRT_WORKERS_DBF = USING_AGGREGATED_GLOBAL_DBF ? 1 : runtime_worker_threads_count;

	/* Hack the start time to make sure demand less than the quantum is also served */
	if((absolute_deadline - start_time) * N_VIRT_WORKERS_DBF < runtime_quantum) start_time = absolute_deadline - runtime_quantum;

	for (int i = ind; i < (N_VIRT_WORKERS_DBF) + ind; i++) {
		assert(global_virt_worker_dbfs);
		void *global_dbf = global_virt_worker_dbfs[i%N_VIRT_WORKERS_DBF];
		global_can_admit = dbf_list_try_add_new_demand(global_dbf, start_time, absolute_deadline, adjustment, sm);
		if (global_can_admit) {
			ind = (i+1)%N_VIRT_WORKERS_DBF;
			return i%N_VIRT_WORKERS_DBF;
		}
		
		if (time_oversupply < dbf_get_time_of_oversupply(global_dbf)) time_oversupply =  dbf_get_time_of_oversupply(global_dbf);
	}
	
	*time_oversupply_p = time_oversupply;
	return -1;
}

uint64_t
traffic_control_decide(struct sandbox_metadata *sandbox_meta, const uint64_t start_time, const uint64_t estimated_execution, int *ret_code, int *worker_id_virtual)
{
	/* Nominal non-zero value in case traffic control is disabled */
	uint64_t work_admitted = estimated_execution;

	int rc = 0;
	int worker_id_v = -1;

	assert(sandbox_meta);
	struct tenant *tenant = sandbox_meta->tenant;
	const uint64_t absolute_deadline = sandbox_meta->absolute_deadline;

	uint64_t time_global_oversupply = 0;
	worker_id_v = global_virt_worker_dbfs_try_update_demand(start_time, estimated_execution, &time_global_oversupply, sandbox_meta);
	bool global_can_admit = worker_id_v >= 0;

	// bool tenant_can_admit = tenant_try_add_job(tenant, start_time, estimated_execution, TRS_CHECK_GUARANTEED, sandbox_meta);
	bool tenant_can_admit = tenant_can_admit_guaranteed(tenant, start_time, estimated_execution);

	if (tenant_can_admit && global_can_admit) {
		/* Case #1: Both the tenant and overall system is under utlized. So, just admit. */
		tenant_can_admit = tenant_try_add_job_as_guaranteed(tenant, start_time, estimated_execution, sandbox_meta);
		assert(tenant_can_admit);
		traffic_control_log_decision(1, true);
		rc = 1;
	} else if (!tenant_can_admit && global_can_admit) {
		/* Case #2: Tenant is over utilized, but system is under utilized. So, admit for work-conservation. */
		if (USING_WORK_CONSERVATION == false) {
			traffic_control_log_decision(2, false);
			dbf_try_update_demand(global_virt_worker_dbfs[worker_id_v], start_time,
												0, absolute_deadline, estimated_execution,
												DBF_DELETE_EXISTING_DEMAND, NULL, sandbox_meta);
			goto any_work_not_admitted;
		}

		traffic_control_log_decision(2, true);
		rc = 2;
	} else if (tenant_can_admit && !global_can_admit) {
		/* Case #3: Tenant is under utilized, but  system is over utilized. So, shed work and then admit. */
		assert(time_global_oversupply >= absolute_deadline);
		
		int worker_id_virt_just_shed;
		while (!global_can_admit) {
			assert(worker_id_v < 0);

			worker_id_virt_just_shed = -1;
			uint64_t cleared_demand = traffic_control_shed_work(tenant, time_global_oversupply, &worker_id_virt_just_shed, false);
			if (cleared_demand == 0) {
				/* No "bad" tenant requests left in the global queue, so we have deny the guaranteed tenant job. */
				traffic_control_log_decision(3, false);
				goto guaranteed_work_not_admitted;
			}

			assert(worker_id_virt_just_shed >= 0);
			void *global_dbf = global_virt_worker_dbfs[worker_id_virt_just_shed];
			global_can_admit = dbf_list_try_add_new_demand(global_dbf, start_time, absolute_deadline, estimated_execution, sandbox_meta);
			time_global_oversupply = dbf_get_time_of_oversupply(global_dbf);
		}

		worker_id_v = worker_id_virt_just_shed;
		tenant_can_admit = tenant_try_add_job_as_guaranteed(tenant, start_time, estimated_execution, sandbox_meta);
		assert(tenant_can_admit);
		traffic_control_log_decision(3, true);
		rc = 1;
	} else if (!tenant_can_admit && !global_can_admit) {
		/* Case #4: Do NOT admit. */

		// printf("Case #4: Do NOT admit.\n");
		// traffic_control_log_decision(4, false);
		// goto any_work_not_admitted;

		// assert(time_global_oversupply >= absolute_deadline);

		int worker_id_virt_just_shed;
		while (!global_can_admit) {
			assert(worker_id_v < 0);

			worker_id_virt_just_shed = -1;

			uint64_t cleared_demand = traffic_control_shed_work(tenant, time_global_oversupply, &worker_id_virt_just_shed, true);
			if (cleared_demand == 0) {
				/* No "bad" tenant requests left in the global queue, so we have deny this new job. */
				traffic_control_log_decision(4, false);
				goto any_work_not_admitted;
			}

			assert(worker_id_virt_just_shed >= 0);
			void *global_dbf = global_virt_worker_dbfs[worker_id_virt_just_shed];
			global_can_admit = dbf_list_try_add_new_demand(global_dbf, start_time, absolute_deadline, estimated_execution, sandbox_meta);
			time_global_oversupply = dbf_get_time_of_oversupply(global_dbf);
		}
// printf("Case #4: Do admit %s.\n", tenant->name);
		assert (global_can_admit);
		worker_id_v = worker_id_virt_just_shed;
		rc = 2;
	}

done:
	*ret_code = rc;
	*worker_id_virtual = worker_id_v;
	return work_admitted;
any_work_not_admitted:
	work_admitted = 0;
	rc = sandbox_meta->exceeded_estimation ? 4295 : 4290;
	goto done;
guaranteed_work_not_admitted:
	work_admitted = 0;
	rc = sandbox_meta->exceeded_estimation ? 4296: 4291;
	goto done;
}


uint64_t traffic_control_shed_work(struct tenant *tenant_to_exclude, uint64_t time_of_oversupply, int *worker_id_virt_just_shed, bool weak_shed)
{
	uint64_t cleared_demand = 0;
	*worker_id_virt_just_shed = -1;
	struct sandbox_metadata *sandbox_meta = NULL;

	struct tenant *tenant_to_punish = tenant_database_find_tenant_most_oversupply(tenant_to_exclude, time_of_oversupply, weak_shed, &sandbox_meta);
	if (tenant_to_punish == NULL) {
		// printf("null\n");
		assert (sandbox_meta == NULL);
		goto done;
	}
	if (tenant_to_punish == tenant_to_exclude) {
		// printf("itself\n");
		// TODO: Should be able to kill from itself???
		goto done;
	}

	assert(sandbox_meta);
	assert(sandbox_meta->tenant == tenant_to_punish);
	assert(sandbox_meta->absolute_deadline <= time_of_oversupply);
	assert(sandbox_meta->terminated == false);
	assert(sandbox_meta->error_code == 0);

	if (sandbox_meta->state == SANDBOX_INITIALIZED) {
		assert(sandbox_meta->tenant_queue == tenant_to_punish->global_sandbox_metas);
		sandbox_meta->error_code = 4090;
		assert(sandbox_meta->owned_worker_idx == -2);
	} else {
		assert(sandbox_meta->tenant_queue == tenant_to_punish->local_sandbox_metas);
		sandbox_meta->error_code = 4091;

		struct message new_message = { 0 };
		if (sandbox_meta->owned_worker_idx >= 0 && sandbox_refs[sandbox_meta->id % RUNTIME_MAX_ALIVE_SANDBOXES]) {
			assert(comm_to_workers);
			struct comm_with_worker *ctw = &comm_to_workers[sandbox_meta->owned_worker_idx];
			assert(ctw);
			assert(ctw->worker_idx == sandbox_meta->owned_worker_idx);
			assert(ck_ring_size(&ctw->worker_ring) < LISTENER_THREAD_RING_SIZE);

			new_message.sandbox_meta = sandbox_meta;
			new_message.sandbox      = sandbox_meta->sandbox_shadow;
			new_message.sandbox_id   = sandbox_meta->id;
			new_message.message_type = MESSAGE_CTW_SHED_CURRENT_JOB;

			if (!ck_ring_enqueue_spsc_message(&ctw->worker_ring, ctw->worker_ring_buffer, &new_message)) {
				panic("Ring buffer was full and the enqueue failed!")
			}
			pthread_kill(runtime_worker_threads[sandbox_meta->owned_worker_idx], SIGALRM);
		}
	}

	struct sandbox_metadata *sm_to_remove = NULL;
	int rc = priority_queue_dequeue_nolock(sandbox_meta->tenant_queue, (void **)&sm_to_remove);
	assert(rc == 0);
	assert(sandbox_meta == sm_to_remove);

	assert(sandbox_meta->trs_job_node == NULL);
	assert(sandbox_meta->remaining_exec > 0);
	assert(sandbox_meta->global_queue_type == 2);
	assert(sandbox_meta->worker_id_virt>=0);

	void *global_dbf = global_virt_worker_dbfs[sandbox_meta->worker_id_virt];
	dbf_list_reduce_demand(sandbox_meta, sandbox_meta->remaining_exec + sandbox_meta->extra_slack, true);
	sandbox_meta->demand_node = NULL;

	cleared_demand = sandbox_meta->remaining_exec;
	// sandbox_meta->remaining_exec = 0;
	// sandbox_meta->extra_slack = 0;
	*worker_id_virt_just_shed = sandbox_meta->worker_id_virt;
	sandbox_meta->terminated = true;
done:
	return cleared_demand;
}

#endif /* TRAFFIC_CONTROL */