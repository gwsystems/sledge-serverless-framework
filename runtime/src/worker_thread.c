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
#include "local_preempted_fifo_queue.h"

/***************************
 * Worker Thread State     *
 **************************/

extern pthread_mutex_t mutexs[1024];
extern pthread_cond_t conds[1024];
extern sem_t semlock[1024];

_Atomic uint32_t local_queue_length[1024] = {0};
uint32_t max_local_queue_length[1024] = {0};
uint32_t max_local_queue_height[1024] = {0};
extern struct perf_window * worker_perf_windows[1024]; // index is worker id
thread_local struct perf_window perf_window_per_thread[1024]; // index is route unique id
struct sandbox* current_sandboxes[1024] = { NULL };
extern uint32_t runtime_worker_group_size;

extern FILE *sandbox_perf_log;
thread_local bool pthread_stop = false;
thread_local int dispatcher_id;
/* context of the runtime thread before running sandboxes or to resume its "main". */
thread_local struct arch_context worker_thread_base_context;

/* Used to index into global arguments and deadlines arrays */
thread_local int global_worker_thread_idx;

/* Mark the worker index within the group */
thread_local int group_worker_thread_idx;

/* Used to track tenants' timeouts */
thread_local struct priority_queue *worker_thread_timeout_queue;
/***********************
 * Worker Thread Logic *
 **********************/

/**
 */

void preallocate_memory() {
	tenant_database_foreach(tenant_preallocate_memory, NULL, NULL);	
}

void perf_window_init() {
	worker_perf_windows[global_worker_thread_idx] = perf_window_per_thread;
	tenant_database_foreach(tenant_perf_window_init, NULL, NULL);
}

void condition_variable_init() {
	pthread_mutex_init(&mutexs[global_worker_thread_idx], NULL);
    	pthread_cond_init(&conds[global_worker_thread_idx], NULL);
}

void semaphore_init(){
	sem_init(&semlock[global_worker_thread_idx], 0, 0);
}

/**
 * The entry function for sandbox worker threads
 * Initializes thread-local state, unmasks signals, sets up epoll loop and
 * @param argument - argument provided by pthread API. We set to -1 on error
 */
void *
worker_thread_main(void *argument)
{
	pthread_setname_np(pthread_self(), "worker");
	/* Set base context as running */
	worker_thread_base_context.variant = ARCH_CONTEXT_VARIANT_RUNNING;

	/* Index was passed via argument */
	global_worker_thread_idx = *(int *)argument;

	atomic_init(&local_queue_length[global_worker_thread_idx], 0);
	/* Set dispatcher id for this worker */
	dispatcher_id = global_worker_thread_idx / runtime_worker_group_size;

	group_worker_thread_idx = global_worker_thread_idx - dispatcher_id * runtime_worker_group_size;

	printf("global thread %d's dispatcher id is %d group size is %d group id is %d\n", global_worker_thread_idx, 
            dispatcher_id, runtime_worker_group_size, group_worker_thread_idx);
	/* Set my priority */
	// runtime_set_pthread_prio(pthread_self(), 2);
	pthread_setschedprio(pthread_self(), -20);

	preallocate_memory();
	perf_window_init();
	condition_variable_init();
	semaphore_init();

	scheduler_runqueue_initialize();
	local_preempted_fifo_queue_init();

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
	software_interrupt_unmask_signal(SIGALRM);
	software_interrupt_unmask_signal(SIGUSR1);

	scheduler_idle_loop();

	//panic("Worker Thread unexpectedly completed idle loop.");
        return NULL;
}
