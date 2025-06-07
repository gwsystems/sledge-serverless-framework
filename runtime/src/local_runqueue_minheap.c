#include <stdint.h>
#include <threads.h>

#include "arch/context.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "global_request_scheduler.h"
#include "local_runqueue.h"
#include "local_runqueue_minheap.h"
#include "panic.h"
#include "priority_queue.h"
#include "sandbox_functions.h"
#include "runtime.h"

extern struct priority_queue* worker_queues[1024];
extern _Atomic uint32_t local_queue_length[1024];
extern uint32_t max_local_queue_length[1024];
extern thread_local int global_worker_thread_idx;
_Atomic uint64_t worker_queuing_cost[1024]; /* index is thread id, each queue's total execution cost of queuing requests */
extern struct perf_window * worker_perf_windows[1024]; /* index is thread id, each queue's perf windows, each queue can 
							  have multiple perf windows */

thread_local static struct priority_queue *local_runqueue_minheap;


/**
 * Checks if the run queue is empty
 * @returns true if empty. false otherwise
 */
bool
local_runqueue_minheap_is_empty()
{
	return priority_queue_length(local_runqueue_minheap) == 0;
}

/**
 * Checks if the run queue is empty
 * @returns true if empty. false otherwise
 */
bool
local_runqueue_minheap_is_empty_index(int index)
{
	struct priority_queue *local_queue = worker_queues[index];
        assert(local_queue != NULL);

        return priority_queue_length(local_queue) == 0;;
}

/**
 * Adds a sandbox to the run queue
 * @param sandbox
 * @returns pointer to sandbox added
 */
void
local_runqueue_minheap_add(struct sandbox *sandbox)
{
	int return_code = priority_queue_enqueue(local_runqueue_minheap, sandbox);
	if (return_code != 0) {
		panic("add request to local queue failed, exit\n");
	}
}

void
local_runqueue_minheap_add_index(int index, struct sandbox *sandbox)
{
	int return_code = priority_queue_enqueue(worker_queues[index], sandbox);
	if (return_code != 0) {
		panic("add request to local queue failed, exit\n");
	}

	atomic_fetch_add(&local_queue_length[index], 1);
	if (local_queue_length[index] > max_local_queue_length[index]) {
                max_local_queue_length[index] = local_queue_length[index];
        }

	uint32_t uid = sandbox->route->admissions_info.uid;

	/* Set estimated exeuction time for the sandbox */
        if (runtime_exponential_service_time_simulation_enabled == false) {
                uint32_t uid = sandbox->route->admissions_info.uid;
                uint64_t estimated_execute_cost = perf_window_get_percentile(&worker_perf_windows[index][uid],
                                                                     sandbox->route->admissions_info.percentile,
                                                                     sandbox->route->admissions_info.control_index);
                /* Use expected execution time in the configuration file as the esitmated execution time
                   if estimated_execute_cost is 0
                */
                if (estimated_execute_cost == 0) {
                        estimated_execute_cost = sandbox->route->expected_execution_cycle;
                }
                sandbox->estimated_cost = estimated_execute_cost;
                sandbox->relative_deadline = sandbox->route->relative_deadline;
        }

	/* Record TS and calcuate RS. SRSF algo:
           1. When reqeust arrives to the queue, record TS and calcuate RS. RS = deadline - execution time
           2. When request starts running, update RS
           3. When request stops, update TS
           4. When request resumes, update RS
        */
        sandbox->srsf_stop_running_ts = __getcycles();
        sandbox->srsf_remaining_slack = sandbox->relative_deadline - sandbox->estimated_cost;
        worker_queuing_cost_increment(index, sandbox->estimated_cost);

}

uint32_t
local_runqueue_minheap_try_add_and_get_len_index(int index, struct sandbox *sandbox, bool *need_interrupt) {
	struct priority_queue *local_queue = worker_queues[index];
        assert(local_queue != NULL);

        if (priority_queue_length(local_queue) == 0) {
                /* The worker is idle */
                *need_interrupt = false;
                return 0;
        } else if (current_sandboxes[index] != NULL &&
                   current_sandboxes[index]->srsf_remaining_slack > 0 &&
                   sandbox_is_preemptable(current_sandboxes[index]) == true &&
                   sandbox_get_priority(sandbox) < sandbox_get_priority(current_sandboxes[index])) {
                /* The new one has a higher priority than the current one, need to interrupt the current one */
                *need_interrupt = true;
                return 0;
        } else {
                /* Current sandbox cannot be interrupted because its priority is higher or its RS is 0, just find
                   a right location to add the new sandbox to the tree
                */
                need_interrupt = false;
                return priority_queue_length(local_queue);
        }
}

uint64_t
local_runqueue_minheap_try_add_and_get_load_index(int index, struct sandbox *sandbox, bool *need_interrupt) {
	struct priority_queue *local_queue = worker_queues[index];
        assert(local_queue != NULL);

        if (priority_queue_length(local_queue) == 0) {
                /* The worker is idle */
                *need_interrupt = false;
                return 0;
        } else if (current_sandboxes[index] != NULL &&
                   current_sandboxes[index]->srsf_remaining_slack > 0 &&
                   sandbox_is_preemptable(current_sandboxes[index]) == true &&
                   sandbox_get_priority(sandbox) < sandbox_get_priority(current_sandboxes[index])) {
                /* The new one has a higher priority than the current one, need to interrupt the current one */
                *need_interrupt = true;
                return 0;
        } else {
                /* Current sandbox cannot be interrupted because its priority is higher or its RS is 0, just find
                   a right location to add the new sandbox to the tree
                */
                need_interrupt = false;
                return worker_queuing_cost[index];
        }
}
/**
 * Deletes a sandbox from the runqueue
 * @param sandbox to delete
 */
static void
local_runqueue_minheap_delete(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	int rc = priority_queue_delete(local_runqueue_minheap, sandbox);
	if (rc == -1) panic("Tried to delete sandbox %lu from runqueue, but was not present\n", sandbox->id);

	atomic_fetch_sub(&local_queue_length[global_worker_thread_idx], 1);
	worker_queuing_cost_decrement(global_worker_thread_idx, sandbox->estimated_cost);
}

/**
 * This function determines the next sandbox to run.
 * This is the head of the local runqueue
 *
 * Execute the sandbox at the head of the thread local runqueue
 * @return the sandbox to execute or NULL if none are available
 */
struct sandbox *
local_runqueue_minheap_get_next()
{
	/* Get the deadline of the sandbox at the head of the local request queue */
	struct sandbox *next = NULL;
	int rc = priority_queue_top(local_runqueue_minheap, (void **)&next);

	if (rc == -ENOENT) return NULL;

	return next;
}

int 
local_runqueue_minheap_get_len() 
{
	return priority_queue_length(local_runqueue_minheap);	
}

int 
local_runqueue_minheap_get_len_index(int index)
{
	struct priority_queue *local_queue = worker_queues[index];
        assert(local_queue != NULL);
	return priority_queue_length(local_queue);
}

/**
 * Registers the PS variant with the polymorphic interface
 */
void
local_runqueue_minheap_initialize()
{
	/* Initialize local state */
	local_runqueue_minheap = priority_queue_initialize(RUNTIME_RUNQUEUE_SIZE, true, sandbox_get_priority);

	worker_queues[global_worker_thread_idx] = local_runqueue_minheap;
	/* Register Function Pointers for Abstract Scheduling API */
	struct local_runqueue_config config = { .add_fn            		= local_runqueue_minheap_add,
						.add_fn_idx        		= local_runqueue_minheap_add_index,
						.try_add_and_get_len_fn_t_idx	= local_runqueue_minheap_try_add_and_get_len_index,
						.try_add_and_get_load_fn_t_idx	= local_runqueue_minheap_try_add_and_get_load_index,
		                                .is_empty_fn       		= local_runqueue_minheap_is_empty,
						.is_empty_fn_idx   		= local_runqueue_minheap_is_empty_index,
		                                .delete_fn         		= local_runqueue_minheap_delete,
						.get_length_fn     		= local_runqueue_minheap_get_len,
						.get_length_fn_idx 		= local_runqueue_minheap_get_len_index,
		                                .get_next_fn       		= local_runqueue_minheap_get_next };

	local_runqueue_initialize(&config);
}
