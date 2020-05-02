#include "sandbox_run_queue_ps.h"
#include "sandbox_run_queue.h"
#include "priority_queue.h"
#include "sandbox_request_scheduler.h"

// Local State
__thread static struct priority_queue sandbox_run_queue_ps;

bool
sandbox_run_queue_ps_is_empty()
{
	return priority_queue_length(&sandbox_run_queue_ps) == 0;
}

/**
 * Pushes a sandbox to the runqueue
 * @param sandbox
 * @returns pointer to request if added. NULL otherwise
 **/
static struct sandbox *
sandbox_run_queue_ps_add(struct sandbox *sandbox)
{
	int return_code = priority_queue_enqueue(&sandbox_run_queue_ps, sandbox);

	return return_code == 0 ? sandbox : NULL;
}

/**
 *
 * @returns A Sandbox Request or NULL
 **/
static struct sandbox *
sandbox_run_queue_ps_remove(void)
{
	return (struct sandbox *)priority_queue_dequeue(&sandbox_run_queue_ps);
}

/**
 *
 * @returns A Sandbox Request or NULL
 **/
static void
sandbox_run_queue_ps_delete(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	int rc = priority_queue_delete(&sandbox_run_queue_ps, sandbox);
	assert(rc != -2);
}

/**
 * This function determines the next sandbox to run. This is either the head of the runqueue or the
 *
 * Execute the sandbox at the head of the thread local runqueue
 * If the runqueue is empty, pull a fresh batch of sandbox requests, instantiate them, and then execute the new head
 * @return the sandbox to execute or NULL if none are available
 **/
struct sandbox *
sandbox_run_queue_ps_get_next()
{
	// At any point, we may need to run the head of the request scheduler, the head of the local runqueue, or we
	// might want to continue executing the current sandbox. If we want to keep executing the current sandbox, we
	// should have a fast path to be able to resume without context switches.

	// If the run queue is empty, we've run the current sandbox to completion

	// We assume that the current sandbox is always on the runqueue when in a runnable state, we know it's the
	// highest priority thing on the runqueue.

	// Case 1: Current runqueue is empty, so pull from global queue and add to runqueue
	if (sandbox_run_queue_is_empty()) {
		sandbox_request_t *sandbox_request = sandbox_request_scheduler_remove();
		if (sandbox_request == NULL) return NULL;
		struct sandbox *sandbox = sandbox_allocate(sandbox_request);
		assert(sandbox);
		free(sandbox_request);
		sandbox->state = RUNNABLE;
		sandbox_run_queue_add(sandbox);
		return sandbox;
	}

	// Case 2: Current runqueue is not empty, so compare head of runqueue to head of global request queue and return
	// highest priority

	uint64_t global_deadline = sandbox_request_scheduler_peek() - SOFTWARE_INTERRUPT_INTERVAL_DURATION_IN_CYCLES;
	// This should be refactored to peek at the top of the runqueue
	struct sandbox *head_of_runqueue = sandbox_run_queue_remove();
	uint64_t        local_deadline   = head_of_runqueue->absolute_deadline;
	sandbox_run_queue_add(head_of_runqueue);

	if (local_deadline <= global_deadline) {
		return head_of_runqueue;
	} else {
		sandbox_request_t *sandbox_request = sandbox_request_scheduler_remove();
		struct sandbox *   sandbox         = sandbox_allocate(sandbox_request);
		assert(sandbox);
		free(sandbox_request);
		sandbox->state = RUNNABLE;
		sandbox_run_queue_add(sandbox);
		debuglog("[%p: %s]\n", sandbox, sandbox->module->name);
		return sandbox;
	}
}


uint64_t
sandbox_get_priority(void *element)
{
	struct sandbox *sandbox = (struct sandbox *)element;
	return sandbox->absolute_deadline;
};

void
sandbox_run_queue_ps_initialize()
{
	// Initialize local state
	priority_queue_initialize(&sandbox_run_queue_ps, sandbox_get_priority);

	// Register Function Pointers for Abstract Scheduling API
	sandbox_run_queue_config_t config = { .add      = sandbox_run_queue_ps_add,
		                              .is_empty = sandbox_run_queue_ps_is_empty,
		                              .remove   = sandbox_run_queue_ps_remove,
		                              .delete   = sandbox_run_queue_ps_delete,
		                              .get_next = sandbox_run_queue_ps_get_next };

	sandbox_run_queue_initialize(&config);
}
