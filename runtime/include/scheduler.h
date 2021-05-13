#pragma once

#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include "client_socket.h"
#include "current_sandbox.h"
#include "global_request_scheduler.h"
#include "global_request_scheduler_deque.h"
#include "global_request_scheduler_minheap.h"
#include "local_runqueue.h"
#include "local_runqueue_minheap.h"
#include "local_runqueue_list.h"
#include "panic.h"
#include "sandbox_request.h"
#include "sandbox_exit.h"
#include "sandbox_functions.h"
#include "sandbox_types.h"
#include "sandbox_set_as_blocked.h"
#include "sandbox_set_as_runnable.h"
#include "sandbox_set_as_running.h"
#include "worker_thread_execute_epoll_loop.h"

enum SCHEDULER
{
	SCHEDULER_FIFO = 0,
	SCHEDULER_EDF  = 1
};

extern enum SCHEDULER scheduler;

static inline struct sandbox *
scheduler_edf_get_next()
{
	assert(!software_interrupt_is_enabled());

	/* Get the deadline of the sandbox at the head of the local request queue */
	struct sandbox *        local          = local_runqueue_get_next();
	uint64_t                local_deadline = local == NULL ? UINT64_MAX : local->absolute_deadline;
	struct sandbox_request *request        = NULL;

	uint64_t global_deadline = global_request_scheduler_peek();

	/* Try to pull and allocate from the global queue if earlier
	 * This will be placed at the head of the local runqueue */
	if (global_deadline < local_deadline) {
		if (global_request_scheduler_remove_if_earlier(&request, local_deadline) == 0) {
			assert(request != NULL);
			assert(request->absolute_deadline < local_deadline);
			struct sandbox *global = sandbox_allocate(request);
			if (!global) goto err_allocate;

			assert(global->state == SANDBOX_INITIALIZED);
			sandbox_set_as_runnable(global, SANDBOX_INITIALIZED);
			local_runqueue_add(global);
		}
	}

/* Return what is at the head of the local runqueue or NULL if empty */
done:
	return local_runqueue_get_next();
err_allocate:
	client_socket_send(request->socket_descriptor, 503);
	client_socket_close(request->socket_descriptor, &request->socket_address);
	free(request);
	goto done;
}

static inline struct sandbox *
scheduler_fifo_get_next()
{
	assert(!software_interrupt_is_enabled());

	struct sandbox *        sandbox         = local_runqueue_get_next();
	struct sandbox_request *sandbox_request = NULL;

	/* If the local runqueue is empty, pull from global request scheduler */
	if (sandbox == NULL) {
		struct sandbox_request *sandbox_request;
		if (global_request_scheduler_remove(&sandbox_request) < 0) goto err;

		sandbox = sandbox_allocate(sandbox_request);
		if (!sandbox) goto err_allocate;

		sandbox_set_as_runnable(sandbox, SANDBOX_INITIALIZED);
		local_runqueue_add(sandbox);
	};

done:
	return sandbox;
err_allocate:
	client_socket_send(sandbox_request->socket_descriptor, 503);
	client_socket_close(sandbox_request->socket_descriptor, &sandbox->client_address);
	free(sandbox_request);
err:
	sandbox = NULL;
	goto done;
}

static inline struct sandbox *
scheduler_get_next()
{
	switch (scheduler) {
	case SCHEDULER_EDF:
		return scheduler_edf_get_next();
	case SCHEDULER_FIFO:
		return scheduler_fifo_get_next();
	default:
		panic("Unimplemented\n");
	}
}

static inline void
scheduler_initialize()
{
	/* Setup Scheduler */
	switch (scheduler) {
	case SCHEDULER_EDF:
		global_request_scheduler_minheap_initialize();
		break;
	case SCHEDULER_FIFO:
		global_request_scheduler_deque_initialize();
		break;
	default:
		panic("Invalid scheduler policy: %u\n", scheduler);
	}
}

static inline void
scheduler_runqueue_initialize()
{
	/* Setup Scheduler */
	switch (scheduler) {
	case SCHEDULER_EDF:
		local_runqueue_minheap_initialize();
		break;
	case SCHEDULER_FIFO:
		local_runqueue_list_initialize();
		break;
	default:
		panic("Invalid scheduler policy: %u\n", scheduler);
	}
}

/**
 * Called by the SIGALRM handler after a quantum
 * Assumes the caller validates that there is something to preempt
 * @param user_context - The context of our user-level Worker thread
 */
static inline void
scheduler_preempt(ucontext_t *user_context)
{
	// If FIFO, just return
	if (scheduler == SCHEDULER_FIFO) return;

	assert(scheduler == SCHEDULER_EDF);
	assert(user_context != NULL);
	assert(!software_interrupt_is_enabled());

	/* Process epoll to make sure that all runnable jobs are considered for execution */
	worker_thread_execute_epoll_loop();

	struct sandbox *current = current_sandbox_get();
	assert(current != NULL);
	assert(current->state == SANDBOX_RUNNING);

	struct sandbox *next = scheduler_get_next();
	assert(next != NULL);

	/* If current equals return, we are already running earliest deadline, so resume execution */
	if (current == next) return;

	/* Save the context of the currently executing sandbox before switching from it */
	sandbox_set_as_runnable(current, SANDBOX_RUNNING);
	arch_mcontext_save(&current->ctxt, &user_context->uc_mcontext);

	/* Update current_sandbox to the next sandbox */
	assert(next->state == SANDBOX_RUNNABLE);
	sandbox_set_as_running(next, SANDBOX_RUNNABLE);
	current_sandbox_set(next);

	/* Update the current deadline of the worker thread */
	runtime_worker_threads_deadline[worker_thread_idx] = next->absolute_deadline;

	/* Restore the context of this sandbox */
	arch_context_restore_new(&user_context->uc_mcontext, &next->ctxt);
}

static inline char *
scheduler_print(enum SCHEDULER variant)
{
	switch (variant) {
	case SCHEDULER_FIFO:
		return "FIFO";
	case SCHEDULER_EDF:
		return "EDF";
	}
}

/**
 * @brief Switches to the next sandbox, placing the current sandbox on the completion queue if in SANDBOX_RETURNED state
 * @param next_sandbox The Sandbox Context to switch to
 */
static inline void
scheduler_switch_to(struct sandbox *next_sandbox)
{
	/* Assumption: The caller disables interrupts */
	assert(!software_interrupt_is_enabled());

	assert(next_sandbox != NULL);
	struct arch_context *next_context = &next_sandbox->ctxt;

	/* Get the old sandbox we're switching from.
	 * This is null if switching from base context
	 */
	struct sandbox *     current_sandbox = current_sandbox_get();
	struct arch_context *current_context = NULL;
	if (current_sandbox != NULL) current_context = &current_sandbox->ctxt;

	assert(next_sandbox != current_sandbox);

	/* If not the current sandbox (which would be in running state), should be runnable */
	assert(next_sandbox->state == SANDBOX_RUNNABLE);

	/* Update the worker's absolute deadline */
	runtime_worker_threads_deadline[worker_thread_idx] = next_sandbox->absolute_deadline;

	if (current_sandbox == NULL) {
		/* Switching from "Base Context" */
#ifdef LOG_CONTEXT_SWITCHES
		debuglog("Base Context (@%p) (%s) > Sandbox %lu (@%p) (%s)\n", &worker_thread_base_context,
		         arch_context_variant_print(worker_thread_base_context.variant), next_sandbox->id, next_context,
		         arch_context_variant_print(next_context->variant));
#endif
	} else {
#ifdef LOG_CONTEXT_SWITCHES
		debuglog("Sandbox %lu (@%p) (%s) > Sandbox %lu (@%p) (%s)\n", current_sandbox->id,
		         &current_sandbox->ctxt, arch_context_variant_print(current_sandbox->ctxt.variant),
		         next_sandbox->id, &next_sandbox->ctxt, arch_context_variant_print(next_context->variant));
#endif

		sandbox_exit(current_sandbox);
	}

	sandbox_set_as_running(next_sandbox, next_sandbox->state);
	current_sandbox_set(next_sandbox);
	arch_context_switch(current_context, next_context);
}


/**
 * @brief Switches to the base context, placing the current sandbox on the completion queue if in RETURNED state
 */
static inline void
scheduler_yield()
{
	assert(!software_interrupt_is_enabled());

	struct sandbox *current_sandbox = current_sandbox_get();
#ifndef NDEBUG
	if (current_sandbox != NULL) {
		assert(current_sandbox->state < SANDBOX_STATE_COUNT);
		assert(current_sandbox->stack_size == current_sandbox->module->stack_size);
	}
#endif

	/* Assumption: Base Context should never switch to Base Context */
	assert(current_sandbox != NULL);
	struct arch_context *current_context = &current_sandbox->ctxt;
	assert(current_context != &worker_thread_base_context);

#ifdef LOG_CONTEXT_SWITCHES
	debuglog("Sandbox %lu (@%p) (%s) > Base Context (@%p) (%s)\n", current_sandbox->id, current_context,
	         arch_context_variant_print(current_sandbox->ctxt.variant), &worker_thread_base_context,
	         arch_context_variant_print(worker_thread_base_context.variant));
#endif

	sandbox_exit(current_sandbox);
	current_sandbox_set(NULL);
	assert(worker_thread_base_context.variant == ARCH_CONTEXT_VARIANT_FAST);
	runtime_worker_threads_deadline[worker_thread_idx] = UINT64_MAX;
	arch_context_switch(current_context, &worker_thread_base_context);
}

/**
 * Mark the currently executing sandbox as blocked, remove it from the local runqueue,
 * and switch to base context
 */
static inline void
scheduler_block(void)
{
	/* Remove the sandbox we were just executing from the runqueue and mark as blocked */
	struct sandbox *current_sandbox = current_sandbox_get();

	/* We might either have blocked in start reading the request or while executing within the WebAssembly
	 * entrypoint. The preemptable flag on the context is used to differentiate. In either case, we should
	 * have disabled interrupts.
	 */
	if (current_sandbox->ctxt.preemptable) software_interrupt_disable();
	assert(!software_interrupt_is_enabled());

	assert(current_sandbox->state == SANDBOX_RUNNING);
	sandbox_set_as_blocked(current_sandbox, SANDBOX_RUNNING);
	generic_thread_dump_lock_overhead();

	scheduler_yield();
}
