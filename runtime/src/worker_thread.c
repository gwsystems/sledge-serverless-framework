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
#include "sandbox_functions.h"
#include "sandbox_set_as_runnable.h"
#include "sandbox_set_as_error.h"
#include "worker_thread.h"

/***************************
 * Worker Thread State     *
 **************************/

/* context of the runtime thread before running sandboxes or to resume its "main". */
__thread struct arch_context worker_thread_base_context;

__thread int worker_thread_epoll_file_descriptor;

/* Used to index into global arguments and deadlines arrays */
__thread int worker_thread_idx;

/***********************
 * Worker Thread Logic *
 **********************/

/**
 * Run all outstanding events in the local thread's epoll loop
 */
static inline void
worker_thread_execute_epoll_loop(void)
{
	assert(software_interrupt_is_disabled);
	while (true) {
		struct epoll_event epoll_events[RUNTIME_MAX_EPOLL_EVENTS];
		int                descriptor_count = epoll_wait(worker_thread_epoll_file_descriptor, epoll_events,
                                                  RUNTIME_MAX_EPOLL_EVENTS, 0);

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

				if (sandbox->state == SANDBOX_BLOCKED) {
					sandbox_set_as_runnable(sandbox, SANDBOX_BLOCKED);
				}
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
 * @param argument - argument provided by pthread API. We set to -1 on error
 */
void *
worker_thread_main(void *argument)
{
	/* The base worker thread should start with software interrupts disabled */
	assert(software_interrupt_is_disabled);

	/* Set base context as running */
	worker_thread_base_context.variant = ARCH_CONTEXT_VARIANT_RUNNING;

	/* Index was passed via argument */
	worker_thread_idx = *(int *)argument;

	/* Set my priority */
	// runtime_set_pthread_prio(pthread_self(), 0);

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

	/* Initialize epoll */
	worker_thread_epoll_file_descriptor = epoll_create1(0);
	if (unlikely(worker_thread_epoll_file_descriptor < 0)) panic_err();

	/* Unmask signals, unless the runtime has disabled preemption */
	if (runtime_preemption_enabled) {
		software_interrupt_unmask_signal(SIGALRM);
		software_interrupt_unmask_signal(SIGUSR1);
	}

	/* Begin Worker Execution Loop */
	struct sandbox *next_sandbox;
	while (true) {
		assert(!software_interrupt_is_enabled());

		/* Assumption: current_sandbox should be unset at start of loop */
		assert(current_sandbox_get() == NULL);

		worker_thread_execute_epoll_loop();

		/* Switch to a sandbox if one is ready to run */
		next_sandbox = local_runqueue_get_next();
		if (next_sandbox != NULL) { sandbox_switch_to(next_sandbox); }
		assert(!software_interrupt_is_enabled());

		/* Clear the completion queue */
		local_completion_queue_free();
	}

	panic("Worker Thread unexpectedly completed run loop.");
}
