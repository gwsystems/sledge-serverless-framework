#include <stdint.h>

#include "current_sandbox.h"
#include "global_request_scheduler.h"
#include "local_runqueue.h"
#include "local_runqueue_minheap.h"
#include "panic.h"
#include "priority_queue.h"
#include "software_interrupt.h"


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
	/* Assumption: Software Interrupts are disabed by caller */
	assert(!software_interrupt_is_enabled());

	struct sandbox *        sandbox = NULL;
	struct sandbox_request *sandbox_request;
	int                     sandbox_rc = local_runqueue_minheap_remove(&sandbox);

	if (sandbox_rc == 0) {
		/* Sandbox ready pulled from local runqueue */

		/* TODO: We remove and immediately re-add sandboxes. This seems like extra work. Consider an API to get
		 * the min without removing it
		 */
		local_runqueue_minheap_add(sandbox);
	} else if (sandbox_rc == -1) {
		/* local runqueue was empty, try to pull a sandbox request and return NULL if we're unable to get one */
		if (global_request_scheduler_remove(&sandbox_request) < 0) goto err;

		/* Try to allocate a sandbox, returning the request on failure */
		sandbox = sandbox_allocate(sandbox_request);
		if (!sandbox) goto sandbox_allocate_err;

		sandbox_set_as_runnable(sandbox);

	} else if (sandbox_rc == -2) {
		/* Unable to take lock, so just return NULL and try later */
		assert(sandbox == NULL);
	}
done:
	return sandbox;
sandbox_allocate_err:
	debuglog("local_runqueue_minheap_get_next failed to allocating sandbox. Readding request to global "
	         "request scheduler\n");
	global_request_scheduler_add(sandbox_request);
err:
	sandbox = NULL;
	goto done;
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

	/* Prevent nested preemption */
	software_interrupt_disable();

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

	/* If we're able to get a sandbox request with a tighter deadline, preempt the current context and run it */
	struct sandbox_request *sandbox_request;
	if (global_deadline < local_deadline) {
		debuglog("Sandbox %lu has deadline of %lu. Trying to preempt for request with %lu\n",
		         current_sandbox->request_arrival_timestamp, local_deadline, global_deadline);

		int return_code = global_request_scheduler_remove(&sandbox_request);

		/* If we were unable to get a sandbox_request, exit */
		if (return_code != 0) goto done;

		debuglog("Preempted %lu for %lu\n", local_deadline, sandbox_request->absolute_deadline);

		/* Allocate the request */
		struct sandbox *next_sandbox = sandbox_allocate(sandbox_request);
		if (!next_sandbox) goto err_sandbox_allocate;

		/* Set as runnable and add it to the runqueue */
		sandbox_set_as_runnable(next_sandbox);

		sandbox_set_as_preempted(current_sandbox);
		/* Save the context of the currently executing sandbox before switching from it */
		arch_mcontext_save(&current_sandbox->ctxt, &user_context->uc_mcontext);

		/* Update current_sandbox to the next sandbox */
		sandbox_set_as_running(next_sandbox);

		/*
		 * Restore the context of this new sandbox
		 * user-level context switch state, so do not enable software interrupts.
		 * TODO: Review the interrupt logic here
		 */
		arch_context_restore(&user_context->uc_mcontext, &next_sandbox->ctxt);
		should_enable_software_interrupt = false;
	}
done:
	if (should_enable_software_interrupt) software_interrupt_enable();
	return;
err_sandbox_allocate:
	assert(sandbox_request);
	debuglog("local_runqueue_minheap_preempt failed to allocate sandbox, returning request to global request "
	         "scheduler\n");
	global_request_scheduler_add(sandbox_request);
err:
	goto done;
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
