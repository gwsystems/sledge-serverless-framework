#include "local_runqueue.h"
#include "local_runqueue_minheap.h"
#include "global_request_scheduler.h"
#include "current_sandbox.h"
#include "priority_queue.h"

#include <stdint.h>

__thread static struct priority_queue local_runqueue_minheap;

/**
 * Checks if the run queue is empty
 * @returns true if empty. false otherwise
 */
bool
local_runqueue_minheap_is_empty()
{
	int length = priority_queue_length(&local_runqueue_minheap);
	assert(length < 5);
	return priority_queue_length(&local_runqueue_minheap) == 0;
}

/**
 * Adds a sandbox to the run queue
 * @param sandbox
 * @returns pointer to request if added. NULL otherwise
 */
static struct sandbox *
local_runqueue_minheap_add(struct sandbox *sandbox)
{
	int original_length = priority_queue_length(&local_runqueue_minheap);

	int return_code = priority_queue_enqueue(&local_runqueue_minheap, sandbox, "Runqueue");
	if (return_code == -1) {
		printf("Thread Runqueue is full!\n");
		exit(EXIT_FAILURE);
	}

	int final_length = priority_queue_length(&local_runqueue_minheap);

	assert(final_length == original_length + 1);

	assert(return_code == 0);
	return sandbox;
}

/**
 * Removes the highest priority sandbox from the run queue
 * @returns A Sandbox or NULL if empty
 */
static struct sandbox *
local_runqueue_minheap_remove()
{
	return (struct sandbox *)priority_queue_dequeue(&local_runqueue_minheap, "Runqueue");
}

/**
 * Deletes a sandbox from the runqueue
 * @param sandbox to delete
 */
static void
local_runqueue_minheap_delete(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	int rc = priority_queue_delete(&local_runqueue_minheap, sandbox, "Runqueue");
	assert(rc == 0);
}

/**
 * This function determines the next sandbox to run. This is either the head of the runqueue or the head of the request
 *queue
 *
 * Execute the sandbox at the head of the thread local runqueue
 * If the runqueue is empty, pull a fresh batch of sandbox requests, instantiate them, and then execute the new head
 * @return the sandbox to execute or NULL if none are available
 */
struct sandbox *
local_runqueue_minheap_get_next()
{
	if (local_runqueue_is_empty()) {
		/* Try to pull a sandbox request and return NULL if we're unable to get one */
		sandbox_request_t *sandbox_request;
		if ((sandbox_request = global_request_scheduler_remove()) == NULL) { return NULL; };

		/* Otherwise, allocate the sandbox request as a runnable sandbox and place on the runqueue */
		struct sandbox *sandbox = sandbox_allocate(sandbox_request);
		if (sandbox == NULL) return NULL;
		assert(sandbox);
		free(sandbox_request);
		sandbox->state = RUNNABLE;
		local_runqueue_minheap_add(sandbox);
		return sandbox;
	}

	/* Resume the sandbox at the top of the runqueue */
	struct sandbox *sandbox = local_runqueue_minheap_remove();
	local_runqueue_minheap_add(sandbox);
	return sandbox;
}


/**
 * Conditionally checks to see if current sandbox should be preempted
 */
void
local_runqueue_minheap_preempt(ucontext_t *user_context)
{
	software_interrupt_disable(); /* no nesting! */

	struct sandbox *current_sandbox = current_sandbox_get();
	/* If current_sandbox is null, there's nothing to preempt, so let the "main" scheduler run its course. */
	if (current_sandbox == NULL) {
		software_interrupt_enable();
		return;
	};

	/* The current sandbox should be the head of the runqueue */
	assert(local_runqueue_minheap_is_empty() == false);

	// TODO: Factor quantum and/or sandbox allocation time into decision
	// uint64_t global_deadline = global_request_scheduler_peek() -
	// SOFTWARE_INTERRUPT_INTERVAL_DURATION_IN_CYCLES;

	bool     should_enable_software_interrupt = true;
	uint64_t local_deadline                   = priority_queue_peek(&local_runqueue_minheap);
	uint64_t global_deadline                  = global_request_scheduler_peek();

	/* Our local deadline should only be ULONG_MAX if our local runqueue is empty */
	if (local_deadline == ULONG_MAX) { assert(local_runqueue_minheap.first_free == 1); };

	/* If we're able to get a sandbox request with a tighter deadline, preempt the current context and run it */
	sandbox_request_t *sandbox_request;
	if (global_deadline < local_deadline && (sandbox_request = global_request_scheduler_remove()) != NULL) {
		printf("Thread %lu Preempted %lu for %lu\n", pthread_self(), local_deadline,
		       sandbox_request->absolute_deadline);

		/* Allocate the request */
		struct sandbox *next_sandbox = sandbox_allocate(sandbox_request);
		assert(next_sandbox);
		free(sandbox_request);
		next_sandbox->state = RUNNABLE;

		/* Add it to the runqueue */
		printf("adding new sandbox to runqueue\n");
		local_runqueue_add(next_sandbox);
		debuglog("[%p: %s]\n", sandbox, sandbox->module->name);

		/* Save the context of the currently executing sandbox before switching from it */
		arch_mcontext_save(&current_sandbox->ctxt, &user_context->uc_mcontext);

		/* Update current_sandbox to the next sandbox */
		current_sandbox_set(next_sandbox);

		/* And load the context of this new sandbox
		RC of 1 indicates that sandbox was last in a user-level context switch state,
		so do not enable software interrupts. */
		if (arch_mcontext_restore(&user_context->uc_mcontext, &next_sandbox->ctxt) == 1)
			should_enable_software_interrupt = false;
	}
	if (should_enable_software_interrupt) software_interrupt_enable();
}


uint64_t
sandbox_get_priority(void *element)
{
	struct sandbox *sandbox = (struct sandbox *)element;
	return sandbox->absolute_deadline;
};

/**
 * Registers the PS variant with the polymorphic interface
 **/
void
local_runqueue_minheap_initialize()
{
	/* Initialize local state */
	priority_queue_initialize(&local_runqueue_minheap, sandbox_get_priority);

	/* Register Function Pointers for Abstract Scheduling API */
	struct local_runqueue_config config = { .add_fn      = local_runqueue_minheap_add,
		                                .is_empty_fn = local_runqueue_minheap_is_empty,
		                                .delete_fn   = local_runqueue_minheap_delete,
		                                .get_next_fn = local_runqueue_minheap_get_next,
		                                .preempt_fn  = local_runqueue_minheap_preempt };

	local_runqueue_initialize(&config);
}
