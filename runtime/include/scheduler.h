#pragma once

#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include "client_socket.h"
#include "current_sandbox.h"
#include "global_request_scheduler.h"
#include "global_request_scheduler_deque.h"
#include "global_request_scheduler_minheap.h"
#include "global_request_scheduler_mts.h"
#include "local_runqueue.h"
#include "local_runqueue_minheap.h"
#include "local_runqueue_list.h"
#include "local_runqueue_mts.h"
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
	SCHEDULER_EDF  = 1,
	SCHEDULER_MTS  = 2
};

extern enum SCHEDULER scheduler;

static inline struct sandbox *
scheduler_mts_get_next()
{
	/* Get the deadline of the sandbox at the head of the local request queue */
	struct sandbox_request * request        = NULL;
	struct sandbox *         local          = local_runqueue_get_next();
	uint64_t                 local_deadline = local == NULL ? UINT64_MAX : local->absolute_deadline;
	enum MULTI_TENANCY_CLASS local_mt_class = MT_DEFAULT;

	if (local) local_mt_class = local->module->pwm_sandboxes[worker_thread_idx].mt_class;

	uint64_t global_guaranteed_deadline = global_request_scheduler_mts_guaranteed_peek();
	uint64_t global_default_deadline    = global_request_scheduler_mts_default_peek();

	/* Try to pull and allocate from the global queue if earlier
	 * This will be placed at the head of the local runqueue */
	switch (local_mt_class) {
	case MT_GUARANTEED:
		if (global_guaranteed_deadline >= local_deadline) goto done;
		break;
	case MT_DEFAULT:
		if (global_guaranteed_deadline == UINT64_MAX && global_default_deadline >= local_deadline) goto done;
		break;
	}

	if (global_request_scheduler_remove_with_mt_class(&request, local_deadline, local_mt_class) == 0) {
		assert(request != NULL);
		struct sandbox *global = sandbox_allocate(request);
		if (!global) goto err_allocate;

		assert(global->state == SANDBOX_INITIALIZED);
		sandbox_set_as_runnable(global, SANDBOX_INITIALIZED);
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
scheduler_edf_get_next()
{
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
	struct sandbox *sandbox = local_runqueue_get_next();

	struct sandbox_request *sandbox_request = NULL;

	if (sandbox == NULL) {
		/* If the local runqueue is empty, pull from global request scheduler */
		if (global_request_scheduler_remove(&sandbox_request) < 0) goto err;

		sandbox = sandbox_allocate(sandbox_request);
		if (!sandbox) goto err_allocate;

		sandbox_set_as_runnable(sandbox, SANDBOX_INITIALIZED);
	} else if (sandbox == current_sandbox_get()) {
		/* Execute Round Robin Scheduling Logic if the head is the current sandbox */
		local_runqueue_list_rotate();
		sandbox = local_runqueue_get_next();
	}


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
	case SCHEDULER_MTS:
		return scheduler_mts_get_next();
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
	switch (scheduler) {
	case SCHEDULER_MTS:
		global_request_scheduler_mts_initialize();
		break;
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
	switch (scheduler) {
	case SCHEDULER_MTS:
		local_runqueue_mts_initialize();
		break;
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
 * Call either at preemptions or blockings to update the Deferrable Server
 *  properties for the given tenant.
 */
static inline void
reduce_module_budget(struct sandbox *sandbox, uint64_t now)
{
	struct module *module = sandbox_get_module(sandbox);

	int64_t  prev_budget           = module->remaining_budget;
	uint64_t duration_of_last_exec = now - sandbox->timestamp_of.last_preemption;

	atomic_fetch_sub(&module->remaining_budget, duration_of_last_exec);
	// if(duration_of_last_exec > runtime_quantum_us*runtime_processor_speed_MHz) debuglog("BEFORE FAA: %ld\nBudget
	// Spent: %ld\nAFTER FAA: RB: %ld %s\n\n", prev_budget, duration_of_last_exec, module->remaining_budget,
	//    (module->remaining_budget <= 0) ? "Oh no! It is NEGATIVE" : "");
	// if (module->remaining_budget < 0) debuglog("OH NO, NEGATIVE!");
	// sandbox->last_preemption_timestamp = now;
}

/**
 * Called by the SIGALRM handler after a quantum
 * Assumes the caller validates that there is something to preempt
 * @param user_context - The context of our user-level Worker thread
 */
static inline void
scheduler_preempt(ucontext_t *user_context)
{
	assert(user_context != NULL);

	struct sandbox *current = current_sandbox_get();
	assert(current != NULL);
	assert(current->state == SANDBOX_RUNNING);

	/* This is for better state-change bookkeeping */
	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - current->timestamp_of.last_state_change;
	current->duration_of_state.running += duration_of_last_state;

	if (scheduler == SCHEDULER_MTS) {
		if (current->module->replenishment_period > 0) reduce_module_budget(current, now);
	}

	/* Process epoll to make sure that all runnable jobs are considered for execution */
	worker_thread_execute_epoll_loop();

	struct sandbox *next = scheduler_get_next();
	assert(next != NULL);

	/* This is for better state-change bookkeeping */
	now                                     = __getcycles();
	current->timestamp_of.last_state_change = now;
	current->timestamp_of.last_preemption   = now;

	if (scheduler == SCHEDULER_MTS) local_timeout_queue_check_for_promotions(now);


	/* If current equals next, no switch is necessary, so resume execution */
	if (current == next) return;

#ifdef LOG_PREEMPTION
	debuglog("Preempting sandbox %lu to run sandbox %lu\n", current->id, next->id);
#endif

	/* Save the context of the currently executing sandbox before switching from it */
	sandbox_set_as_runnable(current, SANDBOX_RUNNING);
	arch_mcontext_save(&current->ctxt, &user_context->uc_mcontext);

	/* Update current_sandbox to the next sandbox */
	assert(next->state == SANDBOX_RUNNABLE);
	sandbox_set_as_running(next, SANDBOX_RUNNABLE);

	switch (next->ctxt.variant) {
	case ARCH_CONTEXT_VARIANT_FAST: {
		arch_context_restore_new(&user_context->uc_mcontext, &next->ctxt);
		break;
	}
	case ARCH_CONTEXT_VARIANT_SLOW: {
		/* Our scheduler restores a fast context when switching to a sandbox that cooperatively yielded
		 * (probably by blocking) or when switching to a freshly allocated sandbox that hasn't yet run.
		 * These conditions can occur in either EDF or FIFO.
		 *
		 * A scheduler restores a slow context when switching to a sandbox that was preempted previously.
		 * Under EDF, a sandbox is only ever preempted by an earlier deadline that either had blocked and since
		 * become runnable or was just freshly allocated. This means that such EDF preemption context switches
		 * should always use a fast context.
		 *
		 * This is not true under FIFO, where there is no innate ordering between sandboxes. A runqueue is
		 * normally only a single sandbox, but it may have multiple sandboxes when one blocks and the worker
		 * pulls an addition request. When the blocked sandbox becomes runnable, the executing sandbox can be
		 * preempted yielding a slow context. This means that FIFO preemption context switches might cause
		 * either a fast or a slow context to be restored during "round robin" execution.
		 */
		assert(scheduler != SCHEDULER_EDF);

		arch_mcontext_restore(&user_context->uc_mcontext, &next->ctxt);
		break;
	}
	default: {
		panic("Unexpectedly tried to switch to a context in %s state\n",
		      arch_context_variant_print(next->ctxt.variant));
	}
	}
}

static inline char *
scheduler_print(enum SCHEDULER variant)
{
	switch (variant) {
	case SCHEDULER_FIFO:
		return "FIFO";
	case SCHEDULER_EDF:
		return "EDF";
	case SCHEDULER_MTS:
		return "MTS";
	}
}

static inline void
scheduler_log_sandbox_switch(struct sandbox *current_sandbox, struct sandbox *next_sandbox)
{
#ifdef LOG_CONTEXT_SWITCHES
	if (current_sandbox == NULL) {
		/* Switching from "Base Context" */
		debuglog("Base Context (@%p) (%s) > Sandbox %lu (@%p) (%s)\n", &worker_thread_base_context,
		         arch_context_variant_print(worker_thread_base_context.variant), next_sandbox->id,
		         &next_sandbox->ctxt, arch_context_variant_print(next_sandbox->ctxt.variant));
	} else {
		debuglog("Sandbox %lu (@%p) (%s) > Sandbox %lu (@%p) (%s)\n", current_sandbox->id,
		         &current_sandbox->ctxt, arch_context_variant_print(current_sandbox->ctxt.variant),
		         next_sandbox->id, &next_sandbox->ctxt, arch_context_variant_print(next_sandbox->ctxt.variant));
	}
#endif
}

/**
 * @brief Switches to the next sandbox, placing the current sandbox on the completion queue if in
 * SANDBOX_RETURNED state
 * @param next_sandbox The Sandbox Context to switch to
 */
static inline void
scheduler_switch_to(struct sandbox *next_sandbox)
{
	assert(next_sandbox != NULL);
	assert(next_sandbox->state == SANDBOX_RUNNABLE);
	struct arch_context *next_context = &next_sandbox->ctxt;

	/* Get the old sandbox we're switching from.
	 * This is null if switching from base context
	 */
	struct sandbox *current_sandbox = current_sandbox_get();
	assert(next_sandbox != current_sandbox);

	struct arch_context *current_context = NULL;
	if (current_sandbox != NULL) {
		current_context = &current_sandbox->ctxt;
		sandbox_exit(current_sandbox);
	}

	scheduler_log_sandbox_switch(current_sandbox, next_sandbox);
	sandbox_set_as_running(next_sandbox, next_sandbox->state);
	arch_context_switch(current_context, next_context);
}


/**
 * @brief Switches to the base context, placing the current sandbox on the completion queue if in RETURNED state
 */
static inline void
scheduler_yield()
{
	struct sandbox *current_sandbox = current_sandbox_get();
	assert(current_sandbox != NULL);

	struct arch_context *current_context = &current_sandbox->ctxt;

	/* Assumption: Base Context should never switch to Base Context */
	assert(current_context != &worker_thread_base_context);

#ifdef LOG_CONTEXT_SWITCHES
	debuglog("Sandbox %lu (@%p) (%s) > Base Context (@%p) (%s)\n", current_sandbox->id, current_context,
	         arch_context_variant_print(current_sandbox->ctxt.variant), &worker_thread_base_context,
	         arch_context_variant_print(worker_thread_base_context.variant));
#endif

	/* Update the MTS props even when a sandbox blocks or completes, too */
	if (current_sandbox->module->replenishment_period > 0) {
		reduce_module_budget(current_sandbox, current_sandbox->timestamp_of.last_state_change);
	}

	sandbox_exit(current_sandbox);
	current_sandbox_set(NULL);
	runtime_worker_threads_deadline[worker_thread_idx] = UINT64_MAX;

	/* Assumption: Base Worker context should never be preempted */
	assert(worker_thread_base_context.variant == ARCH_CONTEXT_VARIANT_FAST);
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

	assert(current_sandbox->state == SANDBOX_RUNNING);
	sandbox_set_as_blocked(current_sandbox, SANDBOX_RUNNING);
	generic_thread_dump_lock_overhead();

	scheduler_yield();
}
