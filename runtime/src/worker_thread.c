#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <stdlib.h>
#include <threads.h>

#include "current_sandbox.h"
#include "memlogging.h"
#include "local_cleanup_queue.h"
#include "local_runqueue.h"
#include "local_runqueue_list.h"
#include "local_runqueue_minheap.h"
#include "panic.h"
#include "runtime.h"
#include "scheduler.h"
#include "worker_thread.h"
#include "tenant_functions.h"
#include "priority_queue.h"

/***************************
 * Worker Thread State     *
 **************************/

extern FILE *sandbox_perf_log;
thread_local bool pthread_stop = false;

/* context of the runtime thread before running sandboxes or to resume its "main". */
thread_local struct arch_context worker_thread_base_context;

/* Used to index into global arguments and deadlines arrays */
thread_local int worker_thread_idx;

/* Used to track tenants' timeouts */
thread_local struct priority_queue *worker_thread_timeout_queue;
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
        mem_log_init2(1024*1024*1024, sandbox_perf_log);

	/* Initialize Cleanup Queue */
	local_cleanup_queue_initialize();

	if (scheduler == SCHEDULER_MTDS) {
		worker_thread_timeout_queue = priority_queue_initialize(RUNTIME_MAX_TENANT_COUNT, false,
		                                                        tenant_timeout_get_priority);
	}

	software_interrupt_unmask_signal(SIGFPE);
	software_interrupt_unmask_signal(SIGSEGV);

	/* Unmask SIGINT signals */
        software_interrupt_unmask_signal(SIGINT);

	/* Unmask signals, unless the runtime has disabled preemption */
	if (runtime_preemption_enabled) {
		//software_interrupt_unmask_signal(SIGALRM);
		//software_interrupt_unmask_signal(SIGUSR1);
	}

	scheduler_idle_loop();

	panic("Worker Thread unexpectedly completed idle loop.");
}
