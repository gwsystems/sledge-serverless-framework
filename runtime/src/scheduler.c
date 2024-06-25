#include "scheduler.h"

enum SCHEDULER scheduler = SCHEDULER_EDF;

void
sandbox_process_scheduler_updates(struct sandbox *sandbox)
{
	if (scheduler == SCHEDULER_MTDS && tenant_is_paid(sandbox->tenant)) {
		atomic_fetch_sub(&sandbox->tenant->remaining_budget, sandbox->last_running_state_duration);
		sandbox->last_running_state_duration = 0;
		return;
	}

#ifdef TRAFFIC_CONTROL
	assert(sandbox->sandbox_meta);
	assert(sandbox == sandbox->sandbox_meta->sandbox_shadow);
	assert(sandbox->id == sandbox->sandbox_meta->id);

	struct comm_with_worker *cfw = &comm_from_workers[worker_thread_idx];
	assert(cfw);

	const uint64_t now = __getcycles();

	struct message new_message = {
		.sandbox                     = sandbox,
		.sandbox_id                  = sandbox->id,
		.sandbox_meta                = sandbox->sandbox_meta,
		.state                       = sandbox->state,
		.sender_worker_idx           = worker_thread_idx,
		.exceeded_estimation         = sandbox->exceeded_estimation,
		.total_running_duration      = 0,
		.timestamp = now
	};

	if (sandbox->state == SANDBOX_RETURNED || sandbox->state == SANDBOX_ERROR) {
		uint64_t adjustment = sandbox->last_running_state_duration;
		if (sandbox->remaining_exec < adjustment) adjustment = sandbox->remaining_exec;
		// const uint64_t adjustment = sandbox->remaining_exec;

		if (USING_LOCAL_RUNQUEUE && adjustment > 0 && sandbox->response_code == 0) {
			// dbf_try_update_demand(worker_dbf, sandbox->timestamp_of.dispatched,
			//                       sandbox->route->relative_deadline, sandbox->absolute_deadline, sandbox->remaining_exec,
			//                       DBF_DELETE_EXISTING_DEMAND, NULL, NULL);
		}
		
		// sandbox->remaining_exec = 0;

		new_message.message_type        = MESSAGE_CFW_DELETE_SANDBOX;
		new_message.adjustment          = adjustment;
		// new_message.remaining_exec = 0;
		new_message.remaining_exec = sandbox->remaining_exec;
		new_message.total_running_duration = sandbox->duration_of_state[SANDBOX_RUNNING_USER] + sandbox->duration_of_state[SANDBOX_RUNNING_SYS];

		if (!ck_ring_enqueue_spsc_message(&cfw->worker_ring, cfw->worker_ring_buffer, &new_message)) {
			panic("Ring The buffer was full and the enqueue operation has failed.!");
		}

		return;
	} 

	/* Unless the sandbox is in the terminal state (handled above), then the only state it can be is INTERRUPTED */
	assert(sandbox->state == SANDBOX_INTERRUPTED);
	assert(sandbox == current_sandbox_get());
	assert(sandbox->response_code == 0);
	assert(sandbox->remaining_exec > 0);
	assert(!sandbox->exceeded_estimation || sandbox->remaining_exec == runtime_quantum);

	if (sandbox->sandbox_meta->terminated) {
        assert(sandbox->sandbox_meta->error_code > 0);
		// dbf_try_update_demand(worker_dbf, sandbox->timestamp_of.dispatched,
		// 	                      sandbox->route->relative_deadline, sandbox->absolute_deadline, sandbox->remaining_exec,
		// 	                      DBF_DELETE_EXISTING_DEMAND, NULL, NULL);
		sandbox->response_code = sandbox->sandbox_meta->error_code;
		interrupted_sandbox_exit();
		return;
	}

	if (sandbox->absolute_deadline < now + (!sandbox->exceeded_estimation ? sandbox->remaining_exec : 0)) {
		// dbf_try_update_demand(worker_dbf, sandbox->timestamp_of.dispatched,
		// 	                      sandbox->route->relative_deadline, sandbox->absolute_deadline, sandbox->remaining_exec,
		// 	                      DBF_DELETE_EXISTING_DEMAND, NULL, NULL);
		sandbox->response_code = 4081;
		interrupted_sandbox_exit();
		return;
	}
	
	dbf_update_mode_t dbf_reduce_mode = DBF_REDUCE_EXISTING_DEMAND;
	uint64_t adjustment = sandbox->last_running_state_duration;
	if (sandbox->remaining_exec < sandbox->last_running_state_duration || sandbox->exceeded_estimation) {
		/* To avoid less than quantum updates manually set the adjustment to quantum */
		adjustment = sandbox->remaining_exec;
		dbf_reduce_mode = DBF_DELETE_EXISTING_DEMAND;
	}

	sandbox->last_running_state_duration = 0;
	sandbox->remaining_exec   -= adjustment;
	
	new_message.adjustment          = adjustment;
	new_message.message_type        = MESSAGE_CFW_REDUCE_DEMAND;
	new_message.remaining_exec = sandbox->remaining_exec;

	if (USING_LOCAL_RUNQUEUE /* && !sandbox->exceeded_estimation */) {
		// dbf_try_update_demand(worker_dbf, sandbox->timestamp_of.dispatched,
		// 						sandbox->route->relative_deadline, sandbox->absolute_deadline, adjustment,
		// 						dbf_reduce_mode, NULL, NULL);
	}

	if (!ck_ring_enqueue_spsc_message(&cfw->worker_ring, cfw->worker_ring_buffer, &new_message)) {
		panic("Ring The buffer was full and the enqueue operation has failed.!")
	}

	if (sandbox->remaining_exec == 0) {
		/* OVERSHOOT case! */
		// printf("Went over estimation - sandbox_id=%lu of %s!\n", sandbox->id, sandbox->tenant->name);
		if (sandbox->exceeded_estimation == false) sandbox->tenant->num_of_overshooted_sandboxes++;
		sandbox->exceeded_estimation = true;
		sandbox->num_of_overshoots++;
		if (sandbox->num_of_overshoots > sandbox->tenant->max_overshoot_of_same_sandbox) {
			sandbox->tenant->max_overshoot_of_same_sandbox = sandbox->num_of_overshoots;
		}
		
		const uint64_t extra_demand = runtime_quantum;

		if (USING_LOCAL_RUNQUEUE && USING_TRY_LOCAL_EXTRA 
			/*&& dbf_try_update_demand(worker_dbf, now, sandbox->route->relative_deadline,
		                          sandbox->absolute_deadline, extra_demand, DBF_CHECK_AND_ADD_DEMAND, &new_message, NULL)*/
								|| (!USING_LOCAL_RUNQUEUE && USING_TRY_LOCAL_EXTRA)) {
			/* Worker DBF has supply left */
			// printf("Worker %d granted extra for sandbox %lu!\n", worker_thread_idx, sandbox->id);

			sandbox->remaining_exec = extra_demand;

			new_message.adjustment = extra_demand;
			new_message.exceeded_estimation = true;
			new_message.message_type = MESSAGE_CFW_EXTRA_DEMAND_REQUEST;
			new_message.remaining_exec = extra_demand;

			if (!ck_ring_enqueue_spsc_message(&cfw->worker_ring, cfw->worker_ring_buffer, &new_message)) {
				panic("Ring The buffer was full and the enqueue operation has failed.!")
			}

			return;
		} else if (USING_WRITEBACK_FOR_OVERSHOOT) { 
			/* Write back */
			// printf("No supply left in worker #%d. So, writeback sandbox=%lu of %s\n", worker_thread_idx, sandbox->id, sandbox->tenant->name);

			sandbox->remaining_exec = 0;
			sandbox->writeback_overshoot_in_progress= true;
			local_runqueue_delete(sandbox); // TODO: This needs to go in preemp_sandbox state change!
			return;
		} else { 
			/* Kill work */
			// printf("No supply left in worker #%d. So, kill sandbox=%lu of %s\n", worker_thread_idx, sandbox->id, sandbox->tenant->name);

            assert(sandbox->response_code == 0);
			sandbox->response_code = 4093;
			interrupted_sandbox_exit();
			return;
		}
	}

#else

	if (sandbox->remaining_exec > sandbox->last_running_state_duration) {
		sandbox->remaining_exec -= sandbox->last_running_state_duration;
	} else {
		sandbox->remaining_exec = 0;
	}
	sandbox->last_running_state_duration = 0;

#endif
}
