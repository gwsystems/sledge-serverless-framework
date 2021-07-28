#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <stdlib.h>

#include "current_sandbox.h"
#include "memlogging.h"
#include "local_completion_queue.h"
#include "local_runqueue.h"
#include "local_runqueue_list.h"
#include "local_runqueue_minheap.h"
#include "panic.h"
#include "runtime.h"
#include "scheduler.h"
#include "worker_thread.h"
#include "worker_thread_execute_epoll_loop.h"

/***************************
 * Worker Thread State     *
 **************************/

/* context of the runtime thread before running sandboxes or to resume its "main". */
__thread struct arch_context worker_thread_base_context;

__thread int worker_thread_epoll_file_descriptor;

/* Used to index into global arguments and deadlines arrays */
__thread int worker_thread_idx;

extern FILE *runtime_sandbox_perf_log;
/***********************
 * Worker Thread Logic *
 **********************/

/**
 * The entry function for sandbox worker threads
 * Initializes thread-local state, unmasks signals, sets up epoll loop and
 * @param argument - argument provided by pthread API. We set to -1 on error
 */
void *
worker_thread_main(void *argument)
{
	/* Set base context as running */
	worker_thread_base_context.variant = ARCH_CONTEXT_VARIANT_RUNNING;

	/* Index was passed via argument */
	worker_thread_idx = *(int *)argument;

	/* Set my priority */
	// runtime_set_pthread_prio(pthread_self(), 2);
	pthread_setschedprio(pthread_self(), -20);

	scheduler_runqueue_initialize();

	/* Initialize memory logging, set 100M memory for logging */
	//mem_log_init2(1024*1024*1024, runtime_sandbox_perf_log);
	mem_log_init2(1024, runtime_sandbox_perf_log);
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

	/* Unmask SIGINT signals */
	software_interrupt_unmask_signal(SIGINT);

	/* Begin Worker Execution Loop */
	struct sandbox *next_sandbox = NULL;
	while (true) {
		/* Assumption: current_sandbox should be unset at start of loop */
		assert(current_sandbox_get() == NULL);

		worker_thread_execute_epoll_loop();

		/* Switch to a sandbox if one is ready to run */
		next_sandbox = scheduler_get_next();
		if (next_sandbox != NULL) { scheduler_switch_to(next_sandbox); }

		/* Clear the completion queue */
		local_completion_queue_free();
	}

	panic("Worker Thread unexpectedly completed run loop.");
}
