#pragma once

#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include "client_socket.h"
#include "current_sandbox.h"
#include "global_request_scheduler.h"
#include "global_request_scheduler_deque.h"
#include "global_request_scheduler_minheap.h"
#include "global_request_scheduler_mtds.h"
#include "local_runqueue.h"
#include "local_runqueue_minheap.h"
#include "local_runqueue_list.h"
#include "local_runqueue_mtds.h"
#include "panic.h"
#include "sandbox_functions.h"
#include "sandbox_types.h"
#include "sandbox_set_as_preempted.h"
#include "sandbox_set_as_runnable.h"
#include "sandbox_set_as_running_sys.h"
#include "sandbox_set_as_interrupted.h"
#include "sandbox_set_as_running_user.h"
#include "scheduler_execute_epoll_loop.h"


/**
 * This scheduler provides for cooperative and preemptive multitasking in a OS process's userspace.
 *
 * When executing cooperatively, the scheduler is directly invoked via `scheduler_cooperative_sched`. It runs a single
 * time in the existing context in order to try to execute a direct sandbox-to-sandbox switch. When no sandboxes are
 * available to execute, the scheduler executes a context switch to `worker_thread_base_context`, which calls
 * `scheduler_cooperative_sched` in an infinite idle loop. If the scheduler needs to restore a sandbox that was
 * previously preempted, it raises a SIGUSR1 signal to enter the scheduler handler to be able to restore the full
 * mcontext structure saved during the last preemption. Otherwise, the cooperative scheduler triggers a "fast switch",
 * which only updates the instruction and stack pointer.
 *
 * Preemptive scheduler is provided by POSIX timers using a set interval defining a scheduling quantum. Our signal
 * handler is configured to mask nested signals. Given that POSIX specifies that the kernel only delivers a SIGALRM to a
 * single thread, the lucky thread that receives the kernel thread has the responsibility of propagating this signal
 * onto all other worker threads. This must occur even when a worker thread is running a sandbox in a nonpreemptable
 * state.
 *
 * When a SIGALRM fires, a worker can be in one of four states:
 *
 * 1) "Running a signal handler" - We mask signals when we are executing a signal handler, which results in signals
 * being ignored. A kernel signal should get delivered to another unmasked worker, so propagation still occurs.
 *
 * 2) "Running the Cooperative Scheduler" - This is signified by the thread local current_sandbox being set to NULL. We
 * propagate the signal and return immediately because we know we're already in the scheduler. We have no sandboxes to
 * interrupt, so no sandbox state transitions occur.
 *
 * 3) "Running a Sandbox in a state other than SANDBOX_RUNNING_USER" - We call sandbox_interrupt on current_sandbox,
 * propagate the sigalrms to the other workers, defer the sigalrm locally, and then return. The SANDBOX_INTERRUPTED
 * timekeeping data is increased to account for the time needed to propagate the sigalrms.
 *
 * 4) "Running a Sandbox in the SANDBOX_RUNNING_USER state - We call sandbox_interrupt on current_sandbox, propagate
 * the sigalrms to the other workers, and then actually enter the scheduler via scheduler_preemptive_sched. The
 * interrupted sandbox may either be preempted or return to depending on the scheduler. If preempted, the interrupted
 * mcontext is saved to the sandbox structure. The SANDBOX_INTERRUPTED timekeeping data is increased to account for the
 * time needed to propagate the sigalrms, run epoll, query the scheduler data structure, and (potentially) allocate and
 * initialize a sandbox.
 */

enum SCHEDULER
{
	SCHEDULER_FIFO = 0,
	SCHEDULER_EDF  = 1,
	SCHEDULER_MTDS = 2
};

extern enum SCHEDULER scheduler;

static inline struct sandbox *
scheduler_mtds_get_next()
{
	/* Get the deadline of the sandbox at the head of the local queue */
	struct sandbox          *local          = local_runqueue_get_next();
	uint64_t                 local_deadline = local == NULL ? UINT64_MAX : local->absolute_deadline;
	enum MULTI_TENANCY_CLASS local_mt_class = MT_DEFAULT;
	struct sandbox          *global         = NULL;

	if (local) local_mt_class = local->module->pwm_sandboxes[worker_thread_idx].mt_class;

	uint64_t global_guaranteed_deadline = global_request_scheduler_mtds_guaranteed_peek();
	uint64_t global_default_deadline    = global_request_scheduler_mtds_default_peek();

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

	if (global_request_scheduler_remove_with_mt_class(&global, local_deadline, local_mt_class) == 0) {
		assert(global != NULL);
		sandbox_prepare_execution_environment(global);
		assert(global->state == SANDBOX_INITIALIZED);
		sandbox_set_as_runnable(global, SANDBOX_INITIALIZED);
	}

/* Return what is at the head of the local runqueue or NULL if empty */
done:
	return local_runqueue_get_next();
}

static inline struct sandbox *
scheduler_edf_get_next()
{
	/* Get the deadline of the sandbox at the head of the local queue */
	struct sandbox *local          = local_runqueue_get_next();
	uint64_t        local_deadline = local == NULL ? UINT64_MAX : local->absolute_deadline;
	struct sandbox *global         = NULL;

	uint64_t global_deadline = global_request_scheduler_peek();

	/* Try to pull and allocate from the global queue if earlier
	 * This will be placed at the head of the local runqueue */
	if (global_deadline < local_deadline) {
		if (global_request_scheduler_remove_if_earlier(&global, local_deadline) == 0) {
			assert(global != NULL);
			assert(global->absolute_deadline < local_deadline);
			sandbox_prepare_execution_environment(global);
			assert(global->state == SANDBOX_INITIALIZED);
			sandbox_set_as_runnable(global, SANDBOX_INITIALIZED);
		}
	}

	/* Return what is at the head of the local runqueue or NULL if empty */
	return local_runqueue_get_next();
}

static inline struct sandbox *
scheduler_fifo_get_next()
{
	struct sandbox *local = local_runqueue_get_next();

	struct sandbox *global = NULL;

	if (local == NULL) {
		/* If the local runqueue is empty, pull from global request scheduler */
		if (global_request_scheduler_remove(&global) < 0) goto done;

		sandbox_prepare_execution_environment(global);
		sandbox_set_as_runnable(global, SANDBOX_INITIALIZED);
	} else if (local == current_sandbox_get()) {
		/* Execute Round Robin Scheduling Logic if the head is the current sandbox */
		local_runqueue_list_rotate();
	}


done:
	return local_runqueue_get_next();
}

static inline struct sandbox *
scheduler_get_next()
{
	switch (scheduler) {
	case SCHEDULER_MTDS:
		return scheduler_mtds_get_next();
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
	case SCHEDULER_MTDS:
		global_request_scheduler_mtds_initialize();
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
	case SCHEDULER_MTDS:
		local_runqueue_mtds_initialize();
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

static inline char *
scheduler_print(enum SCHEDULER variant)
{
	switch (variant) {
	case SCHEDULER_FIFO:
		return "FIFO";
	case SCHEDULER_EDF:
		return "EDF";
	case SCHEDULER_MTDS:
		return "MTDS";
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
	} else if (next_sandbox == NULL) {
		debuglog("Sandbox %lu (@%p) (%s) > Base Context (@%p) (%s)\n", current_sandbox->id,
		         &current_sandbox->ctxt, arch_context_variant_print(current_sandbox->ctxt.variant),
		         &worker_thread_base_context, arch_context_variant_print(worker_thread_base_context.variant));
	} else {
		debuglog("Sandbox %lu (@%p) (%s) > Sandbox %lu (@%p) (%s)\n", current_sandbox->id,
		         &current_sandbox->ctxt, arch_context_variant_print(current_sandbox->ctxt.variant),
		         next_sandbox->id, &next_sandbox->ctxt, arch_context_variant_print(next_sandbox->ctxt.variant));
	}
#endif
}

static inline void
scheduler_preemptive_switch_to(ucontext_t *interrupted_context, struct sandbox *next)
{
	/* Switch to next sandbox */
	switch (next->ctxt.variant) {
	case ARCH_CONTEXT_VARIANT_FAST: {
		assert(next->state == SANDBOX_RUNNABLE);
		arch_context_restore_fast(&interrupted_context->uc_mcontext, &next->ctxt);
		current_sandbox_set(next);
		sandbox_set_as_running_sys(next, SANDBOX_RUNNABLE);
		break;
	}
	case ARCH_CONTEXT_VARIANT_SLOW: {
		assert(next->state == SANDBOX_PREEMPTED);
		arch_context_restore_slow(&interrupted_context->uc_mcontext, &next->ctxt);
		current_sandbox_set(next);
		sandbox_set_as_running_user(next, SANDBOX_PREEMPTED);
		break;
	}
	default: {
		panic("Unexpectedly tried to switch to a context in %s state\n",
		      arch_context_variant_print(next->ctxt.variant));
	}
	}
}

/**
 * Call either at preemptions or blockings to update the Deferrable Server
 *  properties for the given tenant.
 */
static inline void
scheduler_process_policy_specific_updates_on_interrupts()
{
	switch (scheduler) {
	case SCHEDULER_FIFO:
		return;
	case SCHEDULER_EDF:
		return;
	case SCHEDULER_MTDS:
		local_timeout_queue_process_promotions();
		return;
	}
}

/**
 * Called by the SIGALRM handler after a quantum
 * Assumes the caller validates that there is something to preempt
 * @param interrupted_context - The context of our user-level Worker thread
 * @returns the sandbox that the scheduler chose to run
 */
static inline void
scheduler_preemptive_sched(ucontext_t *interrupted_context)
{
	assert(interrupted_context != NULL);

	/* Process epoll to make sure that all runnable jobs are considered for execution */
	scheduler_execute_epoll_loop();

	struct sandbox *interrupted_sandbox = current_sandbox_get();
	assert(interrupted_sandbox != NULL);
	assert(interrupted_sandbox->state == SANDBOX_INTERRUPTED);

	scheduler_process_policy_specific_updates_on_interrupts();

	struct sandbox *next = scheduler_get_next();
	/* Assumption: the current sandbox is on the runqueue, so the scheduler should always return something */
	assert(next != NULL);

	/* If current equals next, no switch is necessary, so resume execution */
	if (interrupted_sandbox == next) {
		sandbox_interrupt_return(interrupted_sandbox, SANDBOX_RUNNING_USER);
		return;
	}

#ifdef LOG_PREEMPTION
	debuglog("Preempting sandbox %lu to run sandbox %lu\n", interrupted_sandbox->id, next->id);
#endif

	/* Preempt executing sandbox */
	scheduler_log_sandbox_switch(interrupted_sandbox, next);
	sandbox_preempt(interrupted_sandbox);

	// Write back global at idx 0
	wasm_globals_set_i64(&interrupted_sandbox->globals, 0, sledge_abi__current_wasm_module_instance.abi.wasmg_0,
	                     true);

	arch_context_save_slow(&interrupted_sandbox->ctxt, &interrupted_context->uc_mcontext);
	scheduler_preemptive_switch_to(interrupted_context, next);
}

/**
 * @brief Switches to the next sandbox
 * Assumption: only called by the "base context"
 * @param next_sandbox The Sandbox to switch to
 */
static inline void
scheduler_cooperative_switch_to(struct arch_context *current_context, struct sandbox *next_sandbox)
{
	assert(current_sandbox_get() == NULL);

	struct arch_context *next_context = &next_sandbox->ctxt;

	/* Switch to next sandbox */
	switch (next_sandbox->state) {
	case SANDBOX_RUNNABLE: {
		assert(next_context->variant == ARCH_CONTEXT_VARIANT_FAST);
		current_sandbox_set(next_sandbox);
		sandbox_set_as_running_sys(next_sandbox, SANDBOX_RUNNABLE);
		break;
	}
	case SANDBOX_PREEMPTED: {
		assert(next_context->variant == ARCH_CONTEXT_VARIANT_SLOW);
		current_sandbox_set(next_sandbox);
		/* arch_context_switch triggers a SIGUSR1, which transitions next_sandbox to running_user */
		break;
	}
	default: {
		panic("Unexpectedly tried to switch to a sandbox in %s state\n",
		      sandbox_state_stringify(next_sandbox->state));
	}
	}
	arch_context_switch(current_context, next_context);
}

static inline void
scheduler_switch_to_base_context(struct arch_context *current_context)
{
	/* Assumption: Base Worker context should never be preempted */
	assert(worker_thread_base_context.variant == ARCH_CONTEXT_VARIANT_FAST);
	arch_context_switch(current_context, &worker_thread_base_context);
}


/* The idle_loop is executed by the base_context. This should not be called directly */
static inline void
scheduler_idle_loop()
{
	while (true) {
		/* Assumption: only called by the "base context" */
		assert(current_sandbox_get() == NULL);

		/* Deferred signals should have been cleared by this point */
		assert(deferred_sigalrm == 0);

		/* Try to wakeup sleeping sandboxes */
		scheduler_execute_epoll_loop();

		/* Switch to a sandbox if one is ready to run */
		struct sandbox *next_sandbox = scheduler_get_next();
		if (next_sandbox != NULL) {
			scheduler_cooperative_switch_to(&worker_thread_base_context, next_sandbox);
		}

		/* Clear the completion queue */
		local_completion_queue_free();
	}
}

/**
 * @brief Used to cooperative switch sandboxes when a sandbox sleeps or exits
 * Because of use-after-free bugs that interfere with our loggers, when a sandbox exits and switches away never to
 * return, the boolean add_to_completion_queue needs to be set to true. Otherwise, we will leak sandboxes.
 * @param add_to_completion_queue - Indicates that the sandbox should be added to the completion queue before switching
 * away
 */
static inline void
scheduler_cooperative_sched(bool add_to_completion_queue)
{
	struct sandbox *exiting_sandbox = current_sandbox_get();
	assert(exiting_sandbox != NULL);

	/* Clearing current sandbox indicates we are entering the cooperative scheduler */
	current_sandbox_set(NULL);
	barrier();
	software_interrupt_deferred_sigalrm_clear();

	struct arch_context *exiting_context = &exiting_sandbox->ctxt;

	/* Assumption: Called by an exiting or sleeping sandbox */
	assert(current_sandbox_get() == NULL);

	/* Deferred signals should have been cleared by this point */
	assert(deferred_sigalrm == 0);

	/* Try to wakeup sleeping sandboxes */
	scheduler_execute_epoll_loop();

	/* We have not added ourself to the completion queue, so we can free */
	local_completion_queue_free();

	/* Switch to a sandbox if one is ready to run */
	struct sandbox *next_sandbox = scheduler_get_next();

	/* If our sandbox slept and immediately woke up, we can just return */
	if (next_sandbox == exiting_sandbox) {
		sandbox_set_as_running_sys(next_sandbox, SANDBOX_RUNNABLE);
		current_sandbox_set(next_sandbox);
		return;
	}

	scheduler_log_sandbox_switch(exiting_sandbox, next_sandbox);

	// Write back global at idx 0
	wasm_globals_set_i64(&exiting_sandbox->globals, 0, sledge_abi__current_wasm_module_instance.abi.wasmg_0, true);

	if (add_to_completion_queue) local_completion_queue_add(exiting_sandbox);
	/* Do not touch sandbox struct after this point! */

	if (next_sandbox != NULL) {
		scheduler_cooperative_switch_to(exiting_context, next_sandbox);
	} else {
		scheduler_switch_to_base_context(exiting_context);
	}
}


static inline bool
scheduler_worker_would_preempt(int worker_idx)
{
	assert(scheduler == SCHEDULER_EDF);
	uint64_t local_deadline  = runtime_worker_threads_deadline[worker_idx];
	uint64_t global_deadline = global_request_scheduler_peek();
	return global_deadline < local_deadline;
}
