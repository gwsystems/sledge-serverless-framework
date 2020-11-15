#include <stdint.h>

#include "client_socket.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "global_request_scheduler.h"
#include "local_runqueue.h"
#include "local_runqueue_minheap.h"
#include "panic.h"
#include "priority_queue.h"
#include "software_interrupt.h"

__thread static struct priority_queue *local_runqueue_minheap;

/**
 * Checks if the run queue is empty
 * @returns true if empty. false otherwise
 */
bool
local_runqueue_minheap_is_empty()
{
	return priority_queue_length_nolock(local_runqueue_minheap) == 0;
}

/**
 * Adds a sandbox to the run queue
 * @param sandbox
 * @returns pointer to sandbox added
 */
void
local_runqueue_minheap_add(struct sandbox *sandbox)
{
	assert(!software_interrupt_is_enabled());

	int return_code = priority_queue_enqueue_nolock(local_runqueue_minheap, sandbox);
	/* TODO: propagate RC to caller. Issue #92 */
	if (return_code == -ENOSPC) panic("Thread Runqueue is full!\n");
}

/**
 * Removes the highest priority sandbox from the run queue
 * @param pointer to test to address of removed sandbox if successful
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty
 */
static int
local_runqueue_minheap_remove(struct sandbox **to_remove)
{
	assert(!software_interrupt_is_enabled());
	return priority_queue_dequeue_nolock(local_runqueue_minheap, (void **)to_remove);
}

/**
 * Deletes a sandbox from the runqueue
 * @param sandbox to delete
 */
static void
local_runqueue_minheap_delete(struct sandbox *sandbox)
{
	assert(!software_interrupt_is_enabled());
	assert(sandbox != NULL);

	int rc = priority_queue_delete_nolock(local_runqueue_minheap, sandbox);
	if (rc == -1) panic("Tried to delete sandbox %lu from runqueue, but was not present\n", sandbox->id);
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
	assert(!software_interrupt_is_enabled());

	struct sandbox *        sandbox         = NULL;
	struct sandbox_request *sandbox_request = NULL;
	int                     sandbox_rc      = priority_queue_top_nolock(local_runqueue_minheap, (void **)&sandbox);

	while (sandbox_rc == -ENOENT && global_request_scheduler_peek() < ULONG_MAX && sandbox == NULL) {
		/* local runqueue empty, try to pull a sandbox request */
		if (global_request_scheduler_remove(&sandbox_request) < 0) {
			/* Assumption: Sandbox request should not be set in case of an error */
			assert(sandbox_request == NULL);
			goto done;
		}

		/* Try to allocate a sandbox. Try again on failure */
		sandbox = sandbox_allocate(sandbox_request);
		if (!sandbox) {
			client_socket_send(sandbox_request->socket_descriptor, 503);
			client_socket_close(sandbox_request->socket_descriptor);
			free(sandbox_request);
			continue;
		};

		assert(sandbox->state == SANDBOX_INITIALIZED);
		sandbox_set_as_runnable(sandbox, SANDBOX_INITIALIZED);
	}

done:
	return sandbox;
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
	uint64_t local_deadline                   = priority_queue_peek(local_runqueue_minheap);
	uint64_t global_deadline                  = global_request_scheduler_peek();
	/* If we're able to get a sandbox request with a tighter deadline, preempt the current context and run it */
	struct sandbox_request *sandbox_request = NULL;
	if (global_deadline < local_deadline) {
#ifdef LOG_PREEMPTION
		debuglog("Sandbox %lu has deadline of %lu. Trying to preempt for request with %lu\n",
		         current_sandbox->id, local_deadline, global_deadline);
#endif

		int return_code = global_request_scheduler_remove_if_earlier(&sandbox_request, local_deadline);

		/* If we were unable to get a sandbox_request, exit */
		if (return_code != 0) {
#ifdef LOG_PREEMPTION
			debuglog("Preemption aborted. Another thread took the request\n");
#endif
			/* Assumption: Sandbox request should not be set in case of an error */
			assert(sandbox_request == NULL);
			goto done;
		}

		assert(sandbox_request->absolute_deadline < local_deadline);

#ifdef LOG_PREEMPTION
		debuglog("Preempted %lu for %lu\n", local_deadline, sandbox_request->absolute_deadline);
#endif

		/* Allocate the request */
		struct sandbox *next_sandbox = sandbox_allocate(sandbox_request);
		if (!next_sandbox) goto err_sandbox_allocate;

		/* Set as runnable and add it to the runqueue */
		assert(next_sandbox->state == SANDBOX_INITIALIZED);
		sandbox_set_as_runnable(next_sandbox, SANDBOX_INITIALIZED);

		assert(current_sandbox->state == SANDBOX_RUNNING);
		sandbox_set_as_preempted(current_sandbox, SANDBOX_RUNNING);

		/* Save the context of the currently executing sandbox before switching from it */
		arch_mcontext_save(&current_sandbox->ctxt, &user_context->uc_mcontext);

		/* Update current_sandbox to the next sandbox */
		assert(next_sandbox->state == SANDBOX_RUNNABLE);
		sandbox_set_as_running(next_sandbox, SANDBOX_RUNNABLE);

		/*
		 * Restore the context of this new sandbox
		 * user-level context switch state, so do not enable software interrupts.
		 * TODO: Review the interrupt logic here. Issue #63
		 */
		arch_context_restore(&user_context->uc_mcontext, &next_sandbox->ctxt);
		should_enable_software_interrupt = false;
	}
done:
	if (should_enable_software_interrupt) software_interrupt_enable();
	return;
err_sandbox_allocate:
	client_socket_send(sandbox_request->socket_descriptor, 503);
	client_socket_close(sandbox_request->socket_descriptor);
	debuglog("local_runqueue_minheap_preempt failed to allocate sandbox\n");
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
 */
void
local_runqueue_minheap_initialize()
{
	/* Initialize local state */
	software_interrupt_disable();
	local_runqueue_minheap = priority_queue_initialize(256, false, sandbox_get_priority);
	software_interrupt_enable();

	/* Register Function Pointers for Abstract Scheduling API */
	struct local_runqueue_config config = { .add_fn      = local_runqueue_minheap_add,
		                                .is_empty_fn = local_runqueue_minheap_is_empty,
		                                .delete_fn   = local_runqueue_minheap_delete,
		                                .get_next_fn = local_runqueue_minheap_get_next,
		                                .preempt_fn  = local_runqueue_minheap_preempt };

	local_runqueue_initialize(&config);
}
