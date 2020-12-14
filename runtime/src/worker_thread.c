#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "client_socket.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "global_request_scheduler.h"
#include "local_completion_queue.h"
#include "local_runqueue.h"
#include "local_runqueue_list.h"
#include "local_runqueue_minheap.h"
#include "panic.h"
#include "runtime.h"
#include "worker_thread.h"

/***************************
 * Worker Thread State     *
 **************************/

/* context of the runtime thread before running sandboxes or to resume its "main". */
__thread struct arch_context worker_thread_base_context;

__thread int worker_thread_epoll_file_descriptor;

/* Total Lock Contention in Cycles */
__thread uint64_t worker_thread_lock_duration;

/* Timestamp when worker thread began executing */
__thread uint64_t worker_thread_start_timestamp;

/***********************
 * Worker Thread Logic *
 **********************/

/**
 * Reports lock contention for the worker thread
 */
static inline void
worker_thread_dump_lock_overhead()
{
#ifndef NDEBUG
#ifdef LOG_LOCK_OVERHEAD
	uint64_t worker_duration = __getcycles() - worker_thread_start_timestamp;
	debuglog("Locks consumed %lu / %lu cycles, or %f%%\n", worker_thread_lock_duration, worker_duration,
	         (double)worker_thread_lock_duration / worker_duration * 100);
#endif
#endif
}

/**
 * Conditionally triggers appropriate state changes for exiting sandboxes
 * @param exiting_sandbox - The sandbox that ran to completion
 */
static inline void
worker_thread_transition_exiting_sandbox(struct sandbox *exiting_sandbox)
{
	assert(exiting_sandbox != NULL);

	switch (exiting_sandbox->state) {
	case SANDBOX_RETURNED:
		/*
		 * We draw a distinction between RETURNED and COMPLETED because a sandbox cannot add itself to the
		 * completion queue
		 */
		sandbox_set_as_complete(exiting_sandbox, SANDBOX_RETURNED);
		break;
	case SANDBOX_BLOCKED:
		/* Cooperative yield, so just break */
		break;
	case SANDBOX_ERROR:
		/* Terminal State, so just break */
		break;
	default:
		panic("Cooperatively switching from a sandbox in a non-terminal %s state\n",
		      sandbox_state_stringify(exiting_sandbox->state));
	}
}

/**
 * @brief Switches to the next sandbox, placing the current sandbox on the completion queue if in SANDBOX_RETURNED state
 * @param next_sandbox The Sandbox Context to switch to
 */
static inline void
worker_thread_switch_to_sandbox(struct sandbox *next_sandbox)
{
	/* Assumption: The caller disables interrupts */
	assert(software_interrupt_is_disabled);

	assert(next_sandbox != NULL);
	struct arch_context *next_context = &next_sandbox->ctxt;

	/* Get the old sandbox we're switching from */
	struct sandbox *current_sandbox = current_sandbox_get();

	if (current_sandbox == NULL) {
		/* Switching from "Base Context" */

		sandbox_set_as_running(next_sandbox, next_sandbox->state);

#ifdef LOG_CONTEXT_SWITCHES
		debuglog("Base Context (@%p) (%s) > Sandbox %lu (@%p) (%s)\n", &worker_thread_base_context,
		         arch_context_variant_print(worker_thread_base_context.variant), next_sandbox->id, next_context,
		         arch_context_variant_print(next_context->variant));
#endif
		/* Assumption: If a slow context switch, current sandbox should be set to the target */
		assert(next_context->variant != ARCH_CONTEXT_VARIANT_SLOW
		       || &current_sandbox_get()->ctxt == next_context);

		arch_context_switch(NULL, next_context);
	} else {
		/* Set the current sandbox to the next */
		assert(next_sandbox != current_sandbox);

		struct arch_context *current_context = &current_sandbox->ctxt;

#ifdef LOG_CONTEXT_SWITCHES
		debuglog("Sandbox %lu (@%p) (%s) > Sandbox %lu (@%p) (%s)\n", current_sandbox->id,
		         &current_sandbox->ctxt, arch_context_variant_print(current_sandbox->ctxt.variant),
		         next_sandbox->id, &next_sandbox->ctxt, arch_context_variant_print(next_context->variant));
#endif

		worker_thread_transition_exiting_sandbox(current_sandbox);

		sandbox_set_as_running(next_sandbox, next_sandbox->state);

#ifndef NDEBUG
		assert(next_context->variant != ARCH_CONTEXT_VARIANT_SLOW
		       || &current_sandbox_get()->ctxt == next_context);
#endif

		/* Switch to the associated context. */
		arch_context_switch(current_context, next_context);
	}

	software_interrupt_enable();
}


/**
 * @brief Switches to the base context, placing the current sandbox on the completion queue if in RETURNED state
 */
static inline void
worker_thread_switch_to_base_context()
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
	debuglog("Sandbox %lu (@%p) (%s)> Base Context (@%p) (%s)\n", current_sandbox->id, current_context,
	         arch_context_variant_print(current_sandbox->ctxt.variant), &worker_thread_base_context,
	         arch_context_variant_print(worker_thread_base_context.variant));
#endif

	worker_thread_transition_exiting_sandbox(current_sandbox);
	current_sandbox_set(NULL);
	assert(worker_thread_base_context.variant == ARCH_CONTEXT_VARIANT_FAST);
	arch_context_switch(current_context, &worker_thread_base_context);
	software_interrupt_enable();
}

/**
 * Mark a blocked sandbox as runnable and add it to the runqueue
 * @param sandbox the sandbox to check and update if blocked
 */
void
worker_thread_wakeup_sandbox(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_BLOCKED);

	software_interrupt_disable();
	sandbox_set_as_runnable(sandbox, SANDBOX_BLOCKED);
	software_interrupt_enable();
}


/**
 * Mark the currently executing sandbox as blocked, remove it from the local runqueue,
 * and switch to base context
 */
void
worker_thread_block_current_sandbox(void)
{
	software_interrupt_disable();

	/* Remove the sandbox we were just executing from the runqueue and mark as blocked */
	struct sandbox *current_sandbox = current_sandbox_get();

	assert(current_sandbox->state == SANDBOX_RUNNING);
	sandbox_set_as_blocked(current_sandbox, SANDBOX_RUNNING);

	/* The worker thread seems to "spin" on a blocked sandbox, so try to execute another sandbox for one quantum
	 * after blocking to give time for the action to resolve */
	struct sandbox *next_sandbox = local_runqueue_get_next();
	if (next_sandbox != NULL) {
		worker_thread_switch_to_sandbox(next_sandbox);
	} else {
		worker_thread_switch_to_base_context();
	};
}


/**
 * Run all outstanding events in the local thread's epoll loop
 */
static inline void
worker_thread_execute_epoll_loop(void)
{
	while (true) {
		struct epoll_event epoll_events[LISTENER_THREAD_MAX_EPOLL_EVENTS];
		int                descriptor_count = epoll_wait(worker_thread_epoll_file_descriptor, epoll_events,
                                                  LISTENER_THREAD_MAX_EPOLL_EVENTS, 0);

		if (descriptor_count < 0) {
			if (errno == EINTR) continue;

			panic_err();
		}

		if (descriptor_count == 0) break;

		for (int i = 0; i < descriptor_count; i++) {
			if (epoll_events[i].events & (EPOLLIN | EPOLLOUT)) {
				/* Re-add to runqueue if blocked */
				struct sandbox *sandbox = (struct sandbox *)epoll_events[i].data.ptr;
				assert(sandbox);

				if (sandbox->state == SANDBOX_BLOCKED) worker_thread_wakeup_sandbox(sandbox);
			} else if (epoll_events[i].events & (EPOLLERR | EPOLLHUP)) {
				/* Mystery: This seems to never fire. Why? Issue #130 */

				/* Close socket and set as error on socket error or unexpected client hangup */
				struct sandbox *sandbox = (struct sandbox *)epoll_events[i].data.ptr;
				int             error   = 0;
				socklen_t       errlen  = sizeof(error);
				getsockopt(epoll_events[i].data.fd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen);

				if (error > 0) {
					debuglog("Socket error: %s", strerror(error));
				} else if (epoll_events[i].events & EPOLLHUP) {
					debuglog("Client Hungup");
				} else {
					debuglog("Unknown Socket error");
				}

				switch (sandbox->state) {
				case SANDBOX_SET_AS_RETURNED:
				case SANDBOX_RETURNED:
				case SANDBOX_SET_AS_COMPLETE:
				case SANDBOX_COMPLETE:
				case SANDBOX_SET_AS_ERROR:
				case SANDBOX_ERROR:
					panic("Expected to have closed socket");
				default:
					client_socket_send(sandbox->client_socket_descriptor, 503);
					sandbox_close_http(sandbox);
					sandbox_set_as_error(sandbox, sandbox->state);
				}
			} else {
				panic("Mystery epoll event!\n");
			};
		}
	}
}

/**
 * The entry function for sandbox worker threads
 * Initializes thread-local state, unmasks signals, sets up epoll loop and
 * @param return_code - argument provided by pthread API. We set to -1 on error
 */
void *
worker_thread_main(void *return_code)
{
	/* Initialize Bookkeeping */
	worker_thread_start_timestamp = __getcycles();
	worker_thread_lock_duration   = 0;

	/* Initialize Base Context as unused
	 * The SP and IP are populated during the first FAST switch away
	 */
	arch_context_init(&worker_thread_base_context, 0, 0);

	/* Initialize Runqueue Variant */
	switch (runtime_scheduler) {
	case RUNTIME_SCHEDULER_EDF:
		local_runqueue_minheap_initialize();
		break;
	case RUNTIME_SCHEDULER_FIFO:
		local_runqueue_list_initialize();
		break;
	default:
		panic("Invalid scheduler policy set: %u\n", runtime_scheduler);
	}

	/* Initialize Completion Queue */
	local_completion_queue_initialize();

	/* Initialize Flags */
	software_interrupt_is_disabled = false;


	/* Unmask signals */
#ifndef PREEMPT_DISABLE
	software_interrupt_unmask_signal(SIGALRM);
	software_interrupt_unmask_signal(SIGUSR1);
#endif
	signal(SIGPIPE, SIG_IGN);

	/* Initialize epoll */
	worker_thread_epoll_file_descriptor = epoll_create1(0);
	if (unlikely(worker_thread_epoll_file_descriptor < 0)) panic_err();

	/* Begin Worker Execution Loop */
	struct sandbox *next_sandbox;
	while (true) {
		/* Assumption: current_sandbox should be unset at start of loop */
		assert(current_sandbox_get() == NULL);

		worker_thread_execute_epoll_loop();

		/* Switch to a sandbox if one is ready to run */
		software_interrupt_disable();
		next_sandbox = local_runqueue_get_next();
		if (next_sandbox != NULL) {
			worker_thread_switch_to_sandbox(next_sandbox);
		} else {
			software_interrupt_enable();
		};

		/* Clear the completion queue */
		local_completion_queue_free();
	}

	panic("Worker Thread unexpectedly completed run loop.");
}

/**
 * Called when the function in the sandbox exits
 * Removes the standbox from the thread-local runqueue, sets its state to SANDBOX_RETURNED,
 * releases the linear memory, and then returns to the base context
 */
__attribute__((noreturn)) void
worker_thread_on_sandbox_exit(struct sandbox *exiting_sandbox)
{
	assert(exiting_sandbox);
	assert(!software_interrupt_is_enabled());
	worker_thread_dump_lock_overhead();
	worker_thread_switch_to_base_context();
	panic("Unexpected return\n");
}
