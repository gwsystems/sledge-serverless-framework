#include <assert.h>
#include <errno.h>

#include "global_request_scheduler.h"
#include "listener_thread.h"
#include "panic.h"
#include "priority_queue.h"
#include "runtime.h"
#include "tenant_functions.h"
#include "sandbox_set_as_error.h"
#include "dbf.h"
#include "local_cleanup_queue.h"

struct priority_queue                *global_request_scheduler_mtdbf;

lock_t global_lock;
// int max_global_runqueue_len = 0; //////////

/**
 * Pushes a sandbox request to the global runqueue
 * @param sandbox
 * @returns pointer to request if added. NULL otherwise
 */
static struct sandbox *
global_request_scheduler_mtdbf_add(struct sandbox *sandbox)
{
	assert(sandbox);
	assert(global_request_scheduler_mtdbf);
	assert(listener_thread_is_running());

	lock_node_t node = {};
	lock_lock(&global_lock, &node);

	int rc = priority_queue_enqueue_nolock(global_request_scheduler_mtdbf, sandbox);
	if (rc != 0) {
		assert(sandbox->response_code == 0);
		sandbox->response_code = 4293;
		sandbox = NULL; // TODO: FIX ME
		goto done;
	} 
	
	sandbox->owned_worker_idx = -1;

	// if(priority_queue_length_nolock(global_request_scheduler_mtdbf) > max_global_runqueue_len) {
	// 	max_global_runqueue_len = priority_queue_length_nolock(global_request_scheduler_mtdbf);
	// 	printf("Global MAX Queue Length: %u\n", max_global_runqueue_len);
	// }
	// printf("GlobalLen: %d, Tenant: %s, Tenant-G: %d, Tenant-L: %d\n\n", priority_queue_length_nolock(global_request_scheduler_mtdbf), sandbox->tenant->name, 
	// 	priority_queue_length_nolock(sandbox->tenant->global_sandbox_metas), priority_queue_length_nolock(sandbox->tenant->local_sandbox_metas));

done: 
	lock_unlock(&global_lock, &node);
	return sandbox;
}

/**
 * @param pointer to the pointer that we want to set to the address of the removed sandbox request
 * @returns 0 if successful, -ENOENT if empty
 */
int
global_request_scheduler_mtdbf_remove(struct sandbox **removed_sandbox)
{
	/* This function won't be used with the MTDS scheduler. Keeping merely for the polymorhism. */
	return -1;
}

/**
 * @param removed_sandbox pointer to set to removed sandbox request
 * @param target_deadline the deadline that the request must be earlier than to dequeue
 * @returns 0 if successful, -ENOENT if empty or if request isn't earlier than target_deadline
 */
int
global_request_scheduler_mtdbf_remove_if_earlier(struct sandbox **removed_sandbox, uint64_t target_deadline)
{
	int rc = -ENOENT;

	const uint64_t now = __getcycles();
	struct sandbox *local = local_runqueue_get_next();

	uint64_t local_rem = local == NULL ? 0 : local->remaining_exec;

	lock_node_t node = {};
	lock_lock(&global_lock, &node);

	struct sandbox_metadata global_metadata = global_request_scheduler_peek_metadata();
	uint64_t global_deadline = global_metadata.absolute_deadline;

	if(USING_EARLIEST_START_FIRST) {
		if (global_deadline - global_metadata.remaining_exec >= target_deadline - local_rem) goto err_enoent;
	} else {
		if (global_deadline >= target_deadline) goto err_enoent;
	}
	// if (global_deadline == UINT64_MAX) goto err_enoent;

	/* Spot the sandbox to remove */
	struct sandbox *top_sandbox = NULL;
	rc = priority_queue_top_nolock(global_request_scheduler_mtdbf, (void **)&top_sandbox);
	assert(top_sandbox);
	assert(top_sandbox->absolute_deadline == global_deadline);
	assert(top_sandbox->remaining_exec == global_metadata.remaining_exec);
	assert(top_sandbox->state == SANDBOX_INITIALIZED || top_sandbox->state == SANDBOX_PREEMPTED);
	assert(top_sandbox->response_code == 0);

	if (top_sandbox->sandbox_meta->terminated) {
		assert(top_sandbox->sandbox_meta->error_code > 0);
		top_sandbox->response_code = top_sandbox->sandbox_meta->error_code;
	} else if (global_deadline < now + (!top_sandbox->exceeded_estimation ? top_sandbox->remaining_exec : 0)) {
		top_sandbox->response_code = top_sandbox->state == SANDBOX_INITIALIZED ? 4080 : 4082;
	} else if (USING_LOCAL_RUNQUEUE) {
		struct tenant *tenant       = top_sandbox->tenant;
		struct route  *route        = top_sandbox->route;

		// assert(dbf_get_worker_idx(worker_dbf) == worker_thread_idx);
		// if (!dbf_try_update_demand(worker_dbf, now, route->relative_deadline,
	    //                        global_deadline, top_sandbox->remaining_exec, DBF_CHECK_AND_ADD_DEMAND, NULL, NULL)) {
		// 	goto err_enoent;
		// }
	}
	else if(local) {
		assert(USING_WRITEBACK_FOR_PREEMPTION);
		assert(local->state == SANDBOX_INTERRUPTED);
		assert(local->writeback_preemption_in_progress == false);
		assert(local->owned_worker_idx >= 0);
		assert(local->pq_idx_in_runqueue >= 1);
		local->writeback_preemption_in_progress = true;
		local_runqueue_delete(local);
		// local->response_code = 5000;
		// interrupted_sandbox_exit();
	} 

	top_sandbox->timestamp_of.dispatched = now; // remove the same op from scheduler validate and set_as_runable 
	top_sandbox->owned_worker_idx = -2;
	// printf("Worker %i accepted a sandbox #%lu!\n", worker_thread_idx, top_sandbox->id);

	rc = priority_queue_dequeue_nolock(global_request_scheduler_mtdbf, (void **)removed_sandbox);
	assert(rc == 0);
	assert(*removed_sandbox == top_sandbox);

	assert(top_sandbox->state == SANDBOX_INITIALIZED || top_sandbox->state == SANDBOX_PREEMPTED);

	lock_unlock(&global_lock, &node);

done:
	return rc;
err_enoent:
	lock_unlock(&global_lock, &node);
	rc = -ENOENT;
	goto done;
}

/**
 * @param removed_sandbox pointer to set to removed sandbox request
 * @param target_deadline the deadline that the request must be earlier than to dequeue
 * @param mt_class the multi-tenancy class of the global request to compare the target deadline against
 * @returns 0 if successful, -ENOENT if empty or if request isn't earlier than target_deadline
 */
int
global_request_scheduler_mtdbf_remove_with_mt_class(struct sandbox **removed_sandbox, uint64_t target_deadline,
                                                    enum MULTI_TENANCY_CLASS target_mt_class)
{
	/* This function won't be used with the MTDBF scheduler. Keeping merely for the polymorhism. */
	return -1;
}

/**
 * Peek at the priority of the highest priority task without having to take the lock
 * Because this is a min-heap PQ, the highest priority is the lowest 64-bit integer
 * This is used to store an absolute deadline
 * @returns value of highest priority value in queue or ULONG_MAX if empty
 */
static uint64_t
global_request_scheduler_mtdbf_peek(void)
{
	return priority_queue_peek(global_request_scheduler_mtdbf);
}


/**
 * Initializes the variant and registers against the polymorphic interface
 */
void
global_request_scheduler_mtdbf_initialize()
{
	global_request_scheduler_mtdbf = priority_queue_initialize_new(RUNTIME_RUNQUEUE_SIZE, false, USING_EARLIEST_START_FIRST ? sandbox_get_priority_global : sandbox_get_priority,
	                                                               global_request_scheduler_update_highest_priority,
	                                                               sandbox_update_pq_idx_in_runqueue);

	lock_init(&global_lock);

	struct global_request_scheduler_config config = {
		.add_fn               = global_request_scheduler_mtdbf_add,
		.remove_fn            = global_request_scheduler_mtdbf_remove,
		.remove_if_earlier_fn = global_request_scheduler_mtdbf_remove_if_earlier,
		.peek_fn              = global_request_scheduler_mtdbf_peek
	};

	global_request_scheduler_initialize(&config);
}

void
global_request_scheduler_mtdbf_free()
{
	priority_queue_free(global_request_scheduler_mtdbf);
}
