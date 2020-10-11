#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <uv.h>

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

#ifdef USE_HTTP_UVIO
/* libuv i/o loop handle per sandboxing thread! */
__thread uv_loop_t worker_thread_uvio_handle;

/* Flag to signify if the thread is currently running callbacks in the libuv event loop */
static __thread bool worker_thread_is_in_libuv_event_loop = false;
#else
__thread int worker_thread_epoll_file_descriptor;
#endif /* USE_HTTP_UVIO */

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
#ifdef DEBUG
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
		debuglog("Base Context (%s) > Sandbox %lu (%s)\n",
		         arch_context_variant_print(worker_thread_base_context.variant), next_sandbox->id,
		         arch_context_variant_print(next_context->variant));
#endif

		arch_context_switch(NULL, next_context);
	} else {
		/* Set the current sandbox to the next */
		assert(next_sandbox != current_sandbox);

		worker_thread_transition_exiting_sandbox(current_sandbox);

		sandbox_set_as_running(next_sandbox, next_sandbox->state);

		struct arch_context *current_context = &current_sandbox->ctxt;

#ifdef LOG_CONTEXT_SWITCHES
		debuglog("Sandbox %lu > Sandbox %lu\n", current_sandbox->id, next_sandbox->id);
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
	worker_thread_transition_exiting_sandbox(current_sandbox);

	/* Assumption: Base Context should never switch to Base Context */
	assert(current_sandbox != NULL);
	assert(&current_sandbox->ctxt != &worker_thread_base_context);

	current_sandbox_set(NULL);

#ifdef LOG_CONTEXT_SWITCHES
	debuglog("Sandbox %lu (%s) > Base Context (%s)\n", current_sandbox->id,
	         arch_context_variant_print(current_sandbox->ctxt.variant),
	         arch_context_variant_print(worker_thread_base_context.variant));
#endif

	arch_context_switch(&current_sandbox->ctxt, &worker_thread_base_context);

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
#ifdef USE_HTTP_UVIO
	assert(worker_thread_is_in_libuv_event_loop == false);
#endif
	software_interrupt_disable();

	/* Remove the sandbox we were just executing from the runqueue and mark as blocked */
	struct sandbox *current_sandbox = current_sandbox_get();

	assert(current_sandbox->state == SANDBOX_RUNNING);
	sandbox_set_as_blocked(current_sandbox, SANDBOX_RUNNING);

	worker_thread_switch_to_base_context();
}


/**
 * Execute I/O
 */
void
worker_thread_process_io(void)
{
#ifdef USE_HTTP_UVIO
#ifdef USE_HTTP_SYNC
	/*
	 * TODO: realistically, we're processing all async I/O on this core when a sandbox blocks on http processing,
	 * not great! if there is a way, perhaps RUN_ONCE and check if your I/O is processed, if yes,
	 * return else do async block! Issue #98
	 */
	uv_run(worker_thread_get_libuv_handle(), UV_RUN_DEFAULT);
#else  /* USE_HTTP_SYNC */
	worker_thread_block_current_sandbox();
#endif /* USE_HTTP_UVIO */
#else
	assert(false);
	/* it should not be called if not using uvio for http */
#endif
}

/**
 * Run all outstanding events in the local thread's libuv event loop
 */
void
worker_thread_execute_libuv_event_loop(void)
{
#ifdef USE_HTTP_UVIO
	worker_thread_is_in_libuv_event_loop = true;
	int n = uv_run(worker_thread_get_libuv_handle(), UV_RUN_NOWAIT), i = 0;
	while (n > 0) {
		n--;
		uv_run(worker_thread_get_libuv_handle(), UV_RUN_NOWAIT);
	}
	worker_thread_is_in_libuv_event_loop = false;
#endif
	return;
}

/**
 * Run all outstanding events in the local thread's libuv event loop
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
					sandbox_close_http(sandbox);
					sandbox_set_as_error(sandbox, sandbox->state);
				}
			};
		}
	}
}

static inline void
worker_thread_initialize_async_io()
{
#ifdef USE_HTTP_UVIO
	worker_thread_is_in_libuv_event_loop = false;
	/* Initialize libuv event loop handle */
	uv_loop_init(&worker_thread_uvio_handle);
#else
	/* Initialize epoll */
	worker_thread_epoll_file_descriptor = epoll_create1(0);
	if (unlikely(worker_thread_epoll_file_descriptor < 0)) panic_err();
#endif
}
static inline void
worker_thread_process_async_io()
{
#ifdef USE_HTTP_UVIO
	/* Execute libuv event loop */
	if (!worker_thread_is_in_libuv_event_loop) worker_thread_execute_libuv_event_loop();
#else
	/* Execute non-blocking epoll_wait to add sandboxes back on the runqueue */
	worker_thread_execute_epoll_loop();
#endif
}

/**
 * The entry function for sandbox worker threads
 * Initializes thread-local state, unmasks signals, sets up libuv loop and
 * @param return_code - argument provided by pthread API. We set to -1 on error
 */
void *
worker_thread_main(void *return_code)
{
	/* Initialize Bookkeeping */
	worker_thread_start_timestamp = __getcycles();
	worker_thread_lock_duration   = 0;

	/* Initialize Base Context */
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

	worker_thread_initialize_async_io();

	/* Begin Worker Execution Loop */
	struct sandbox *next_sandbox;
	while (true) {
		/* Assumption: current_sandbox should be unset at start of loop */
		assert(current_sandbox_get() == NULL);

		worker_thread_process_async_io();

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
