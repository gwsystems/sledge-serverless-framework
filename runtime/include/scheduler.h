#pragma once

#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include "current_sandbox.h"
#include "global_request_scheduler.h"
#include "global_request_scheduler_deque.h"
#include "global_request_scheduler_minheap.h"
#include "global_request_scheduler_mtds.h"
#include "global_request_scheduler_mtdbf.h"
#include "local_cleanup_queue.h"
#include "local_runqueue.h"
#include "local_runqueue_list.h"
#include "local_runqueue_minheap.h"
#include "local_runqueue_mtds.h"
#include "local_runqueue_mtdbf.h"
#include "panic.h"
#include "sandbox_functions.h"
#include "sandbox_set_as_interrupted.h"
#include "sandbox_set_as_preempted.h"
#include "sandbox_set_as_runnable.h"
#include "sandbox_set_as_running_sys.h"
#include "sandbox_set_as_running_user.h"
#include "sandbox_types.h"
#include "sandbox_set_as_error.h"
#include "scheduler_options.h"


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

static inline struct sandbox *
scheduler_mtdbf_get_next()
{
	/* Get the deadline of the sandbox at the head of the local queue */
	struct sandbox *local          = local_runqueue_get_next();
	uint64_t        local_deadline = local == NULL ? UINT64_MAX : local->absolute_deadline;

	uint64_t local_rem = local == NULL ? 0 : local->remaining_exec;
	struct sandbox *global         = NULL;
	uint64_t now = __getcycles();

	struct sandbox_metadata global_metadata = global_request_scheduler_peek_metadata();

	/* Try to pull and allocate from the global queue if earlier
	 * This will be placed at the head of the local runqueue */
	if(USING_EARLIEST_START_FIRST) {
		if (global_metadata.absolute_deadline - global_metadata.remaining_exec >= local_deadline - local_rem) goto done;
	} else {
		if (global_metadata.absolute_deadline >= local_deadline) goto done;
	}

	if (global_request_scheduler_remove_if_earlier(&global, local_deadline) == 0) {
		assert(global != NULL);
		// assert(global->absolute_deadline < local_deadline);
		if (sandbox_validate_self_lifetime(global) == 0) {
			if (global->state == SANDBOX_INITIALIZED) {
				sandbox_prepare_execution_environment(global);
				sandbox_set_as_runnable(global, SANDBOX_INITIALIZED);
				
				struct comm_with_worker *cfw = &comm_from_workers[worker_thread_idx];
				assert(cfw);

				struct message new_message = {
					.sandbox                     = global,
					.sandbox_id                  = global->id,
					.sandbox_meta                = global->sandbox_meta,
					.state                       = global->state,
					.sender_worker_idx           = worker_thread_idx,
					.exceeded_estimation         = global->exceeded_estimation,
					.message_type                = MESSAGE_CFW_PULLED_NEW_SANDBOX,
					.timestamp = now
				};

				if (!ck_ring_enqueue_spsc_message(&cfw->worker_ring, cfw->worker_ring_buffer, &new_message)) {
					panic("Ring The buffer was full and the enqueue operation has failed.!");
				}
			} else {
				assert(global->state == SANDBOX_PREEMPTED);
				// debuglog("Resuming writeback\n");
				local_runqueue_add(global);
				// global->owned_worker_idx = worker_thread_idx;
			}

			assert(global->state == SANDBOX_RUNNABLE || global->state == SANDBOX_PREEMPTED);
			// printf("Worker %i accepted a sandbox #%lu!\n", worker_thread_idx, global->id);
		}
	}

done:
	/* Return what is at the head of the local runqueue or NULL if empty */
	return local_runqueue_get_next();
}

static inline struct sandbox *
scheduler_mtds_get_next()
{
	/* Get the deadline of the sandbox at the head of the local queue */
	struct sandbox          *local          = local_runqueue_get_next();
	uint64_t                 local_deadline = local == NULL ? UINT64_MAX : local->absolute_deadline;
	enum MULTI_TENANCY_CLASS local_mt_class = MT_DEFAULT;
	struct sandbox          *global         = NULL;

	if (local) local_mt_class = local->tenant->pwt_sandboxes[worker_thread_idx].mt_class;

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

	if (global_request_scheduler_mtds_remove_with_mt_class(&global, local_deadline, local_mt_class) == 0) {
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
scheduler_sjf_get_next()
{
	struct sandbox *local          = local_runqueue_get_next();
	uint64_t        local_rem_exec = local == NULL ? UINT64_MAX : local->remaining_exec;
	struct sandbox *global         = NULL;

	uint64_t global_remaining_exec = global_request_scheduler_peek();

	/* Try to pull and allocate from the global queue if earlier
	 * This will be placed at the head of the local runqueue */
	if (global_remaining_exec < local_rem_exec) {
		if (global_request_scheduler_remove_if_earlier(&global, local_rem_exec) == 0) {
			assert(global != NULL);
			assert(global->remaining_exec < local_rem_exec);
			sandbox_prepare_execution_environment(global);
			assert(global->state == SANDBOX_INITIALIZED);
			sandbox_set_as_runnable(global, SANDBOX_INITIALIZED);
		}
	}

	/* Return what is at the head of the local runqueue or NULL if empty */
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
	case SCHEDULER_MTDBF:
		return scheduler_mtdbf_get_next();
	case SCHEDULER_MTDS:
		return scheduler_mtds_get_next();
	case SCHEDULER_SJF:
		return scheduler_sjf_get_next();
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
	case SCHEDULER_MTDBF:
		global_request_scheduler_mtdbf_initialize();
		break;
	case SCHEDULER_MTDS:
		global_request_scheduler_mtds_initialize();
		break;
	case SCHEDULER_EDF:
	case SCHEDULER_SJF:
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
	case SCHEDULER_MTDBF:
		local_runqueue_mtdbf_initialize();
		break;
	case SCHEDULER_MTDS:
		local_runqueue_mtds_initialize();
		break;
	case SCHEDULER_EDF:
	case SCHEDULER_SJF:
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
	case SCHEDULER_SJF:
		return "SJF";
	case SCHEDULER_MTDS:
		return "MTDS";
	case SCHEDULER_MTDBF:
		return "MTDBF";
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
	/* Switch to base context */
	if (next == NULL) {
		arch_context_restore_fast(&interrupted_context->uc_mcontext, &worker_thread_base_context);
		current_sandbox_set(NULL);
		return;
	}

	/* Switch to next sandbox */
	switch (next->ctxt.variant) {
	case ARCH_CONTEXT_VARIANT_FAST: {
		assert(next->state == SANDBOX_RUNNABLE);
		arch_context_restore_fast(&interrupted_context->uc_mcontext, &next->ctxt);
		current_sandbox_set(next);
assert(sledge_abi__current_wasm_module_instance.abi.memory.id == next->id);
		sandbox_set_as_running_sys(next, SANDBOX_RUNNABLE);
		break;
	}
	case ARCH_CONTEXT_VARIANT_SLOW: {
		assert(next->state == SANDBOX_PREEMPTED);
		arch_context_restore_slow(&interrupted_context->uc_mcontext, &next->ctxt);
		current_sandbox_set(next);
assert(sledge_abi__current_wasm_module_instance.abi.memory.id == next->id);
		sandbox_set_as_running_user(next, SANDBOX_PREEMPTED);
		break;
	}
	default: {
		panic("Unexpectedly tried to switch to a context in %s state\n",
		      arch_context_variant_print(next->ctxt.variant));
	}
	}
}

static inline int
scheduler_check_messages_from_listener()
{
	int rc = 0;

	assert(comm_to_workers);

	struct message           new_message = { 0 };
	struct comm_with_worker *ctw         = &comm_to_workers[worker_thread_idx];
	assert(ctw);
	assert(ctw->worker_idx == worker_thread_idx);
	assert(ck_ring_size(&ctw->worker_ring) < LISTENER_THREAD_RING_SIZE);

	while (ck_ring_dequeue_spsc_message(&ctw->worker_ring, ctw->worker_ring_buffer, &new_message)) {
		assert(new_message.message_type == MESSAGE_CTW_SHED_CURRENT_JOB);
		/* Check if the sandbox is still alive (not freed yet) */
		if (sandbox_refs[new_message.sandbox_id % RUNTIME_MAX_ALIVE_SANDBOXES]) {
			struct sandbox *sandbox_to_kill = new_message.sandbox;
			assert(sandbox_to_kill);
			assert(sandbox_to_kill->id == new_message.sandbox_id);

			if (sandbox_to_kill->pq_idx_in_runqueue == 0 || sandbox_to_kill->owned_worker_idx != worker_thread_idx) {
			/* Make sure the sandbox is in a non-terminal or asleep state (aka: still in the runqueue) */
				new_message.sandbox = NULL;
				new_message.sandbox_id = 0;
				continue;
			}

			struct sandbox_metadata *sandbox_meta = sandbox_to_kill->sandbox_meta;
			assert(sandbox_meta);
			assert(sandbox_meta->sandbox_shadow == sandbox_to_kill);
			assert(sandbox_meta->id == sandbox_to_kill->id);
			assert(sandbox_meta->error_code > 0);

			// printf("Worker#%d shedding sandbox #%lu\n", worker_thread_idx, sandbox_to_kill->id);
			assert(sandbox_to_kill->response_code == 0);
			sandbox_to_kill->response_code = sandbox_meta->error_code;
			sandbox_exit_error(sandbox_to_kill);
			local_cleanup_queue_add(sandbox_to_kill);
		}
		new_message.sandbox = NULL;
		new_message.sandbox_id = 0;
	}

	return rc;
}

/**
 * Call either at preemptions or blockings to update the scheduler-specific
 *  properties for the given tenant.
 */
static inline void
scheduler_process_policy_specific_updates_on_interrupts(struct sandbox *interrupted_sandbox)
{
	switch (scheduler) {
	case SCHEDULER_FIFO:
	case SCHEDULER_EDF:
	case SCHEDULER_SJF:
		sandbox_process_scheduler_updates(interrupted_sandbox);
		return;
	case SCHEDULER_MTDS:
		sandbox_process_scheduler_updates(interrupted_sandbox);
		local_timeout_queue_process_promotions();
		return;
	case SCHEDULER_MTDBF:
		scheduler_check_messages_from_listener();
		if (interrupted_sandbox->state != SANDBOX_ERROR) {
			sandbox_process_scheduler_updates(interrupted_sandbox);
		}
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

	struct sandbox *interrupted_sandbox = current_sandbox_get();
	assert(interrupted_sandbox != NULL);
	assert(interrupted_sandbox->state == SANDBOX_INTERRUPTED);

	scheduler_process_policy_specific_updates_on_interrupts(interrupted_sandbox);

	struct sandbox *next = scheduler_get_next();

	/* Assumption: the current sandbox is still there, even if the worker had to shed it from its runqueue above */
	assert(interrupted_sandbox != NULL);

	if (interrupted_sandbox->state == SANDBOX_ERROR) goto done;
	if(!(interrupted_sandbox->state == SANDBOX_INTERRUPTED)) {
		printf("sand state: %u\n", interrupted_sandbox->state);
	}
	assert(interrupted_sandbox->state == SANDBOX_INTERRUPTED);

	/* Assumption: the current sandbox is on the runqueue, so the scheduler should always return something */
	// assert(next != NULL); // Cannot assert, since the head of the global queue may have expired and cleaned before this

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

	// Update global at idx 0
	int rc = wasm_globals_set_i64(&interrupted_sandbox->globals, 0,
							sledge_abi__current_wasm_module_instance.abi.wasmg_0, true);
	assert(rc == 0);
	
	arch_context_save_slow(&interrupted_sandbox->ctxt, &interrupted_context->uc_mcontext);

#ifdef TRAFFIC_CONTROL
	if (USING_WRITEBACK_FOR_PREEMPTION || USING_WRITEBACK_FOR_OVERSHOOT) {
		struct message new_message = {
			.sandbox             = interrupted_sandbox,
			.sandbox_id          = interrupted_sandbox->id,
			.sandbox_meta        = interrupted_sandbox->sandbox_meta,
			.state               = interrupted_sandbox->state,
			.sender_worker_idx   = worker_thread_idx,
			.exceeded_estimation = interrupted_sandbox->exceeded_estimation,
			.timestamp           = interrupted_sandbox->timestamp_of.last_state_change,
			.remaining_exec = interrupted_sandbox->remaining_exec
		};

		if (interrupted_sandbox->writeback_overshoot_in_progress) {
			assert(USING_WRITEBACK_FOR_OVERSHOOT);
			assert(interrupted_sandbox->remaining_exec == 0);
			new_message.message_type = MESSAGE_CFW_WRITEBACK_OVERSHOOT;
			new_message.adjustment = runtime_quantum;
		}
		else if (interrupted_sandbox->writeback_preemption_in_progress) {
			assert(USING_WRITEBACK_FOR_PREEMPTION);
			assert(USING_LOCAL_RUNQUEUE == false);
			new_message.message_type = MESSAGE_CFW_WRITEBACK_PREEMPTION;
			new_message.adjustment = 0;
		} else panic("No writeback is in progress. Cannot be here!");

		struct comm_with_worker *cfw = &comm_from_workers[worker_thread_idx];
		if (!ck_ring_enqueue_spsc_message(&cfw->worker_ring, cfw->worker_ring_buffer, &new_message)) {
			panic("Ring The buffer was full and the enqueue operation has failed.!");
		}
	}
#endif

	/* CAUTION! Worker MUST NOT access interrupted sandbox after this point! */
done:
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
assert(sledge_abi__current_wasm_module_instance.abi.memory.id == next_sandbox->id);
		sandbox_set_as_running_sys(next_sandbox, SANDBOX_RUNNABLE);
		break;
	}
	case SANDBOX_PREEMPTED: {
		assert(next_context->variant == ARCH_CONTEXT_VARIANT_SLOW);
		current_sandbox_set(next_sandbox);
assert(sledge_abi__current_wasm_module_instance.abi.memory.id == next_sandbox->id);
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
	int spin = 0, max_spin = 0;
	while (true) {
		/* Assumption: only called by the "base context" */
		assert(current_sandbox_get() == NULL);

		/* Deferred signals should have been cleared by this point */
		assert(deferred_sigalrm == 0);

		/* Switch to a sandbox if one is ready to run */
		struct sandbox *next_sandbox = scheduler_get_next();
		if (next_sandbox != NULL) {
			scheduler_cooperative_switch_to(&worker_thread_base_context, next_sandbox);
			spin++;
			if (spin > max_spin) {
				max_spin = spin;
				// printf("Worker #%d max useless spins #%d!\n", worker_thread_idx, max_spin);
			}
		} else {
			spin = 0;
		}

		/* Clear the cleanup queue */
		local_cleanup_queue_free();

		/* Improve the performance of spin-wait loops (works only if preemptions enabled) */
		if (runtime_worker_spinloop_pause_enabled) pause();
	}
}

/**
 * @brief Used to cooperative switch sandboxes when a sandbox sleeps or exits
 * Because of use-after-free bugs that interfere with our loggers, when a sandbox exits and switches away never to
 * return, the boolean add_to_cleanup_queue needs to be set to true. Otherwise, we will leak sandboxes.
 * @param add_to_cleanup_queue - Indicates that the sandbox should be added to the cleanup queue before switching
 * away
 */
static inline void
scheduler_cooperative_sched(bool add_to_cleanup_queue)
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

	/* We have not added ourself to the cleanup queue, so we can free */
	local_cleanup_queue_free();

	/* Switch to a sandbox if one is ready to run */
	struct sandbox *next_sandbox = scheduler_get_next();

	/* If our sandbox slept and immediately woke up, we can just return */
	if (next_sandbox == exiting_sandbox) {
		assert(0); // Never happens, sandboxes don't sleep anymore
		sandbox_set_as_running_sys(next_sandbox, SANDBOX_RUNNABLE);
		current_sandbox_set(next_sandbox);
		return;
	}

	scheduler_log_sandbox_switch(exiting_sandbox, next_sandbox);

	// Write back global at idx 0
	assert(sledge_abi__current_wasm_module_instance.abi.wasmg_0 == 0);
	wasm_globals_set_i64(&exiting_sandbox->globals, 0, sledge_abi__current_wasm_module_instance.abi.wasmg_0, true);

	if (add_to_cleanup_queue) local_cleanup_queue_add(exiting_sandbox);
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
	// assert(scheduler == SCHEDULER_EDF);
	uint64_t local_deadline  = runtime_worker_threads_deadline[worker_idx];
	uint64_t global_deadline = global_request_scheduler_peek();
	
	/* Only send a worker SIGARLM if it has a sandbox to execute (MTDBF) 
	    or it needs to check the global queue for a new higher priority job */
	return local_deadline < UINT64_MAX || global_deadline < local_deadline;
}
