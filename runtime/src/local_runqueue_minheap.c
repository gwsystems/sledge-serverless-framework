#include <stdint.h>

#include "current_sandbox.h"
#include "global_request_scheduler.h"
#include "local_runqueue.h"
#include "local_runqueue_minheap.h"
#include "panic.h"
#include "priority_queue.h"


__thread static struct priority_queue local_runqueue_minheap;

/**
 * Checks if the run queue is empty
 * @returns true if empty. false otherwise
 */
bool
local_runqueue_minheap_is_empty()
{
	return priority_queue_is_empty(&local_runqueue_minheap);
}

/**
 * Adds a sandbox to the run queue
 * @param sandbox
 * @returns pointer to sandbox added
 */
void
local_runqueue_minheap_add(struct sandbox *sandbox)
{
	int return_code = priority_queue_enqueue(&local_runqueue_minheap, sandbox);
	/* TODO: propagate RC to caller */
	if (return_code == -1) panic("Thread Runqueue is full!\n");
}

/**
 * Removes the highest priority sandbox from the run queue
 * @param pointer to test to address of removed sandbox if successful
 * @returns 0 if successful, -1 if empty, -2 if unable to take lock
 */
static int
local_runqueue_minheap_remove(struct sandbox **to_remove)
{
	return priority_queue_dequeue(&local_runqueue_minheap, (void **)to_remove);
}

/**
 * Deletes a sandbox from the runqueue
 * @param sandbox to delete
 */
static void
local_runqueue_minheap_delete(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	int rc = priority_queue_delete(&local_runqueue_minheap, sandbox);
	if (rc == -1) {
		panic("Err: Thread Local %lu tried to delete sandbox %lu from runqueue, but was not present\n",
		      pthread_self(), sandbox->request_arrival_timestamp);
	}
}

/**
 * This function determines the next sandbox to run.
 * This is either the head of the runqueue or the head of the request queue
 *
 * Execute the sandbox at the head of the thread local runqueue
 * If the runqueue is empty, pull a fresh batch of sandbox requests, instantiate them, and then execute the new head
 * @return the sandbox to execute or NULL if none are available
 */
struct sandbox *
local_runqueue_minheap_get_next()
{
	struct sandbox *sandbox    = NULL;
	int             sandbox_rc = local_runqueue_minheap_remove(&sandbox);

	if (sandbox_rc == 0) {
		/* Sandbox ready pulled from local runqueue */

		/* TODO: We remove and immediately re-add sandboxes. This seems like extra work. Consider an API to get
		 * the min without removing it
		 */
		local_runqueue_minheap_add(sandbox);
	} else if (sandbox_rc == -1) {
		/* local runqueue was empty, try to pull a sandbox request and return NULL if we're unable to get one */
		struct sandbox_request *sandbox_request;
		int                     sandbox_request_rc = global_request_scheduler_remove(&sandbox_request);
		if (sandbox_request_rc != 0) return NULL;

		sandbox = sandbox_allocate(sandbox_request);
		assert(sandbox);
		free(sandbox_request);
		sandbox->state = SANDBOX_RUNNABLE;
		local_runqueue_minheap_add(sandbox);
	} else if (sandbox_rc == -2) {
		/* Unable to take lock, so just return NULL and try later */
		assert(sandbox == NULL);
	}
	return sandbox;
}


/**
 * Called by the SIGALRM handler after a quantum
 * Assumes the caller validates that there is something to preempt
 * @param user_context - The context of our user-level Worker thread
 */
void
local_runqueue_minheap_preempt(ucontext_t *user_context)
{
	assert(user_context != NULL);

	software_interrupt_disable(); /* no nesting! */

	struct sandbox *current_sandbox = current_sandbox_get();

	/* If current_sandbox is null, there's nothing to preempt, so let the "main" scheduler run its course. */
	if (current_sandbox == NULL) {
		software_interrupt_enable();
		return;
	};

	/* The current sandbox should be the head of the runqueue */
	assert(local_runqueue_minheap_is_empty() == false);

	bool     should_enable_software_interrupt = true;
	uint64_t local_deadline                   = priority_queue_peek(&local_runqueue_minheap);
	uint64_t global_deadline                  = global_request_scheduler_peek();

	/* Our local deadline should only be ULONG_MAX if our local runqueue is empty */
	if (local_deadline == ULONG_MAX) { assert(local_runqueue_minheap.first_free == 1); };

	/*
	 * If we're able to get a sandbox request with a tighter deadline, preempt the current context and run it
	 *
	 * TODO: Factor quantum and/or sandbox allocation time into decision
	 * Something like global_request_scheduler_peek() - software_interrupt_interval_duration_in_cycles;
	 */

	if (global_deadline < local_deadline) {
		debuglog("Thread %lu | Sandbox %lu | Had deadline of %lu. Trying to preempt for request with %lu\n",
		         pthread_self(), current_sandbox->allocation_timestamp, local_deadline, global_deadline);

		struct sandbox_request *sandbox_request;
		int                     return_code = global_request_scheduler_remove(&sandbox_request);

		// If we were unable to get a sandbox_request, exit
		if (return_code != 0) goto done;

		debuglog("Thread %lu Preempted %lu for %lu\n", pthread_self(), local_deadline,
		         sandbox_request->absolute_deadline);
		/* Allocate the request */
		struct sandbox *next_sandbox = sandbox_allocate(sandbox_request);
		assert(next_sandbox);
		free(sandbox_request);
		next_sandbox->state = SANDBOX_RUNNABLE;

		/* Add it to the runqueue */
		local_runqueue_add(next_sandbox);
		debuglog("[%p: %s]\n", sandbox, sandbox->module->name);

		/* Save the context of the currently executing sandbox before switching from it */
		arch_mcontext_save(&current_sandbox->ctxt, &user_context->uc_mcontext);

		/* Update current_sandbox to the next sandbox */
		current_sandbox_set(next_sandbox);

		/*
		 * And load the context of this new sandbox
		 * RC of 1 indicates that sandbox was last in a user-level context switch state,
		 * so do not enable software interrupts.
		 */
		if (arch_mcontext_restore(&user_context->uc_mcontext, &next_sandbox->ctxt) == 1)
			should_enable_software_interrupt = false;
	}
done:
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
