#include "sandbox_run_queue_ps.h"
#include "sandbox_run_queue.h"
#include "priority_queue.h"
#include "sandbox_request_scheduler.h"
#include "current_sandbox.h"

#include <stdint.h>

// Local State
__thread static struct priority_queue sandbox_run_queue_ps;

bool
sandbox_run_queue_ps_is_empty()
{
	int length = priority_queue_length(&sandbox_run_queue_ps);
	assert(length < 5);
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
	int original_length = priority_queue_length(&sandbox_run_queue_ps);

	int return_code = priority_queue_enqueue(&sandbox_run_queue_ps, sandbox, "Runqueue");
	if (return_code == -1) {
		printf("Thread Runqueue is full!\n");
		exit(EXIT_FAILURE);
	}

	int final_length = priority_queue_length(&sandbox_run_queue_ps);

	assert(final_length == original_length + 1);

	// printf("Added Sandbox to runqueue\n");
	assert(return_code == 0);
	return sandbox;
}

/**
 *
 * @returns A Sandbox or NULL
 **/
static struct sandbox *
sandbox_run_queue_ps_remove(void)
{
	return (struct sandbox *)priority_queue_dequeue(&sandbox_run_queue_ps, "Runqueue");
}

/**
 * @returns A Sandbox or NULL
 **/
static void
sandbox_run_queue_ps_delete(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	int rc = priority_queue_delete(&sandbox_run_queue_ps, sandbox, "Runqueue");
	assert(rc == 0);
}

/**
 * This function determines the next sandbox to run. This is either the head of the runqueue or the head of the request
 *queue
 *
 * Execute the sandbox at the head of the thread local runqueue
 * If the runqueue is empty, pull a fresh batch of sandbox requests, instantiate them, and then execute the new head
 * @return the sandbox to execute or NULL if none are available
 **/
struct sandbox *
sandbox_run_queue_ps_get_next()
{
	if (sandbox_run_queue_is_empty()) {
		// Try to pull a sandbox request and return NULL if we're unable to get one
		sandbox_request_t *sandbox_request;
		if ((sandbox_request = sandbox_request_scheduler_remove()) == NULL) {
			// printf("Global Request Queue was empty!\n");
			return NULL;
		};

		// Otherwise, allocate the sandbox request as a runnable sandbox and place on the runqueue
		struct sandbox *sandbox = sandbox_allocate(sandbox_request);
		// If sandbox is NULL, we failed to allocate, so the request wasn't actually serviced
		// Should we re-add this to the request queue?
		free(sandbox_request);
		if (sandbox != NULL) sandbox_set_as_runnable(sandbox, NULL);
		return sandbox;
	}

	// Resume the sandbox at the top of the runqueue
	struct sandbox *sandbox = sandbox_run_queue_ps_remove();
	sandbox_run_queue_ps_add(sandbox);
	return sandbox;
}

/**
 * Called by the SIGALRM handler after a quantum
 * Assumes the caller validates that there is something to preempt
 * @param user_context - The context of our user-level Worker thread
 **/
void
sandbox_run_queue_ps_preempt(ucontext_t *user_context)
{
	software_interrupt_disable(); // no nesting!
	struct sandbox *current_sandbox = current_sandbox_get();
	assert(current_sandbox != NULL);
	assert(sandbox_run_queue_ps_is_empty() == false);
	assert(user_context != NULL);


	bool     should_enable_software_interrupt = true;
	uint64_t local_deadline                   = priority_queue_peek(&sandbox_run_queue_ps);
	uint64_t global_deadline                  = sandbox_request_scheduler_peek();

	// Our local deadline should only be ULONG_MAX if our local runqueue is empty
	if (local_deadline == ULONG_MAX) { assert(sandbox_run_queue_ps.first_free == 1); };

	// If we're able to get a sandbox request with a tighter deadline, preempt the current context and run it
	// TODO: Factor quantum and/or sandbox allocation time into decision
	// uint64_t global_deadline = sandbox_request_scheduler_peek() -
	// SOFTWARE_INTERRUPT_INTERVAL_DURATION_IN_CYCLES;
	sandbox_request_t *sandbox_request;
	if (global_deadline < local_deadline && (sandbox_request = sandbox_request_scheduler_remove()) != NULL) {
		printf("Thread %lu Preempted %lu for %lu\n", pthread_self(), local_deadline,
		       sandbox_request->absolute_deadline);

		// Allocate the request
		struct sandbox *next_sandbox = sandbox_allocate(sandbox_request);
		assert(next_sandbox);
		sandbox_set_as_runnable(next_sandbox, NULL);
		assert(next_sandbox->state == SANDBOX_RUNNABLE);
		free(sandbox_request);

		// Save the context of the currently executing sandbox before switching from it
		sandbox_set_as_preempted(current_sandbox, &user_context->uc_mcontext);

		// Set as Running conditionally enables interrupts
		sandbox_set_as_running(next_sandbox, &user_context->uc_mcontext);
	} else {
		software_interrupt_enable();
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
		                              .delete   = sandbox_run_queue_ps_delete,
		                              .get_next = sandbox_run_queue_ps_get_next,
		                              .preempt  = sandbox_run_queue_ps_preempt };

	sandbox_run_queue_initialize(&config);
}
