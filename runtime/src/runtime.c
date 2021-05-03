#include <signal.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <arpa/inet.h>

#include "admissions_control.h"
#include "arch/context.h"
#include "client_socket.h"
#include "debuglog.h"
#include "global_request_scheduler_deque.h"
#include "global_request_scheduler_minheap.h"
#include "http_parser_settings.h"
#include "http_response.h"
#include "listener_thread.h"
#include "module.h"
#include "runtime.h"
#include "sandbox_request.h"
#include "software_interrupt.h"

/***************************
 * Shared Process State    *
 **************************/

pthread_t runtime_worker_threads[RUNTIME_WORKER_THREAD_CORE_COUNT];
int       runtime_worker_threads_argument[RUNTIME_WORKER_THREAD_CORE_COUNT] = { 0 };
/* The active deadline of the sandbox running on each worker thread */
uint64_t runtime_worker_threads_deadline[RUNTIME_WORKER_THREAD_CORE_COUNT] = { UINT64_MAX };

/******************************************
 * Shared Process / Listener Thread Logic *
 *****************************************/

void
runtime_cleanup()
{
	if (runtime_sandbox_perf_log != NULL) fflush(runtime_sandbox_perf_log);

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
	char *   resource_names[] = { "RLIMIT_DATA", "RLIMIT_NOFILE" };

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
			printf("\t%s: %s\n", resource_names[i], max);
		} else {
			limit.rlim_cur = limit.rlim_max;
			if (setrlimit(resource, &limit) < 0) panic_err();
			printf("\t%s: %s (Increased from %s)\n", resource_names[i], max, lim);
		}
	}
}

/**
 * Initialize runtime global state, mask signals, and init http parser
 */
void
runtime_initialize(void)
{
	http_total_init();
	sandbox_request_count_initialize();
	sandbox_count_initialize();

	/* Setup Scheduler */
	switch (runtime_scheduler) {
	case RUNTIME_SCHEDULER_EDF:
		global_request_scheduler_minheap_initialize();
		break;
	case RUNTIME_SCHEDULER_FIFO:
		global_request_scheduler_deque_initialize();
		break;
	default:
		panic("Invalid scheduler policy set: %u\n", runtime_scheduler);
	}

	/* Configure Signals */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, runtime_cleanup);
	/* These should only be unmasked by workers */
	software_interrupt_mask_signal(SIGUSR1);
	software_interrupt_mask_signal(SIGALRM);

	http_parser_settings_initialize();
	admissions_control_initialize();
}
