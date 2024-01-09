#include <arpa/inet.h>
#include <assert.h>
#include <signal.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>

#include "admissions_control.h"
#include "arch/context.h"
#include "debuglog.h"
#include "global_request_scheduler_deque.h"
#include "global_request_scheduler_minheap.h"
#include "http_parser_settings.h"
#include "listener_thread.h"
#include "module.h"
#include "runtime.h"
#include "sandbox_total.h"
#include "scheduler.h"
#include "software_interrupt.h"
#include "sandbox_perf_log.h"

#define WAKEUP_THREAD_OVERHEAD 5 /* 5us */

/***************************
 * Shared Process State    *
 **************************/

/* Count of the total number of requests we've ever received. Never decrements as it is used to dispatch requests to workers with RR */
_Atomic uint64_t request_index;
pthread_t *runtime_worker_threads;
pthread_t *runtime_listener_threads;
int       *runtime_worker_threads_argument;
int       *runtime_listener_threads_argument;
/* The active deadline of the sandbox running on each worker thread */
uint64_t *runtime_worker_threads_deadline;
uint64_t wakeup_thread_cycles;

/******************************************
 * Shared Process / Listener Thread Logic *
 *****************************************/

void
runtime_cleanup()
{
	sandbox_perf_log_cleanup();
	http_session_perf_log_cleanup();

	if (runtime_worker_threads_deadline) free(runtime_worker_threads_deadline);
	if (runtime_worker_threads_argument) free(runtime_worker_threads_argument);
	if (runtime_worker_threads) {
		free(runtime_worker_threads);
		runtime_worker_threads = NULL;
	}
	if (runtime_listener_threads) {
		free(runtime_listener_threads);
		runtime_listener_threads = NULL;
	}

	software_interrupt_cleanup();
	exit(EXIT_SUCCESS);
}

/**
 * Sets the process data segment (RLIMIT_DATA) and # file descriptors
 * (RLIMIT_NOFILE) soft limit to its hard limit (see man getrlimit)
 */
void
runtime_set_resource_limits_to_max()
{
	struct rlimit limit;
	const size_t  uint64_t_max_digits = 20;
	char          lim[uint64_t_max_digits + 1];
	char          max[uint64_t_max_digits + 1];

	uint64_t resources[]      = { RLIMIT_DATA, RLIMIT_NOFILE };
	char    *resource_names[] = { "RLIMIT_DATA", "RLIMIT_NOFILE" };

	for (int i = 0; i < sizeof(resources) / sizeof(resources[0]); i++) {
		int resource = resources[i];
		if (getrlimit(resource, &limit) < 0) panic_err();

		if (limit.rlim_cur == RLIM_INFINITY) {
			strncpy(lim, "Infinite", uint64_t_max_digits);
		} else {
			snprintf(lim, uint64_t_max_digits, "%lu", limit.rlim_cur);
		}
		if (limit.rlim_max == RLIM_INFINITY) {
			strncpy(max, "Infinite", uint64_t_max_digits);
		} else {
			snprintf(max, uint64_t_max_digits, "%lu", limit.rlim_max);
		}
		if (limit.rlim_cur == limit.rlim_max) {
			pretty_print_key_value(resource_names[i], "%s\n", max);
		} else {
			limit.rlim_cur = limit.rlim_max;
			if (setrlimit(resource, &limit) < 0) panic_err();
			pretty_print_key_value(resource_names[i], "%s (Increased from %s)\n", max, lim);
		}
	}
}

/**
 * Initialize runtime global state, mask signals, and init http parser
 */
void
runtime_initialize(void)
{
	runtime_worker_threads = calloc(runtime_worker_threads_count, sizeof(pthread_t));
	assert(runtime_worker_threads != NULL);
	runtime_worker_threads_argument = calloc(runtime_worker_threads_count, sizeof(int));
	assert(runtime_worker_threads_argument != NULL);
	runtime_worker_threads_deadline = malloc(runtime_worker_threads_count * sizeof(uint64_t));
	assert(runtime_worker_threads_deadline != NULL);
	memset(runtime_worker_threads_deadline, UINT8_MAX, runtime_worker_threads_count * sizeof(uint64_t));

	runtime_listener_threads = calloc(runtime_listener_threads_count, sizeof(pthread_t));
        assert(runtime_listener_threads != NULL);
	runtime_listener_threads_argument = calloc(runtime_listener_threads_count, sizeof(int));
	assert(runtime_listener_threads_argument != NULL);

	http_total_init();
	sandbox_total_initialize();
	request_index_initialize();
	sandbox_state_totals_initialize();
	worker_queuing_cost_initialize();

	/* Setup Scheduler */
	scheduler_initialize();

	/* Configure Signals */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, runtime_cleanup);
	signal(SIGQUIT, runtime_cleanup);

	http_parser_settings_initialize();
	admissions_control_initialize();
	wakeup_thread_cycles = WAKEUP_THREAD_OVERHEAD * runtime_processor_speed_MHz; 
}

static void
runtime_call_getrlimit(int id, char *name)
{
	struct rlimit rl;

	if (getrlimit(id, &rl)) {
		perror("getrlimit: ");
		exit(-1);
	}
}

static void
runtime_call_setrlimit(int id, rlim_t c, rlim_t m)
{
	struct rlimit rl;

	rl.rlim_cur = c;
	rl.rlim_max = m;
	if (setrlimit(id, &rl)) {
		perror("getrlimit: ");
		exit(-1);
	}
}

void
runtime_set_pthread_prio(pthread_t thread, unsigned int nice)
{
	struct sched_param sp;
	int                policy;

	runtime_call_getrlimit(RLIMIT_CPU, "CPU");
	runtime_call_getrlimit(RLIMIT_RTTIME, "RTTIME");
	runtime_call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
	runtime_call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
	runtime_call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
	runtime_call_getrlimit(RLIMIT_NICE, "NICE");

	if (pthread_getschedparam(thread, &policy, &sp) < 0) { perror("pth_getparam: "); }
	sp.sched_priority = sched_get_priority_max(SCHED_RR) - nice;
	if (pthread_setschedparam(thread, SCHED_RR, &sp) < 0) {
		perror("pth_setparam: ");
		exit(-1);
	}
	if (pthread_getschedparam(thread, &policy, &sp) < 0) { perror("getparam: "); }
	assert(sp.sched_priority == sched_get_priority_max(SCHED_RR) - nice);

	return;
}

void
runtime_set_prio(unsigned int nice)
{
	struct sched_param sp;

	runtime_call_getrlimit(RLIMIT_CPU, "CPU");
	runtime_call_getrlimit(RLIMIT_RTTIME, "RTTIME");
	runtime_call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
	runtime_call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
	runtime_call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");

	runtime_call_getrlimit(RLIMIT_NICE, "NICE");

	if (sched_getparam(0, &sp) < 0) { perror("getparam: "); }
	sp.sched_priority = sched_get_priority_max(SCHED_RR) - nice;
	if (sched_setscheduler(0, SCHED_RR, &sp) < 0) {
		perror("setscheduler: ");
		exit(-1);
	}
	if (sched_getparam(0, &sp) < 0) { perror("getparam: "); }
	assert(sp.sched_priority == sched_get_priority_max(SCHED_RR) - nice);

	return;
}
