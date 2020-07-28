#include <ctype.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include "module.h"
#include "panic.h"
#include "runtime.h"
#include "sandbox.h"
#include "software_interrupt.h"
#include "worker_thread.h"

/* Conditionally used by debuglog when DEBUG is set */
#ifdef DEBUG
int32_t runtime_log_file_descriptor = -1;
#endif

float    runtime_processor_speed_MHz                          = 0;
uint32_t runtime_total_online_processors                      = 0;
uint32_t runtime_total_worker_processors                      = 0;
uint32_t runtime_first_worker_processor                       = 0;
int runtime_worker_threads_argument[WORKER_THREAD_CORE_COUNT] = { 0 }; /* The worker sets its argument to -1 on error */
pthread_t runtime_worker_threads[WORKER_THREAD_CORE_COUNT];


/**
 * Returns instructions on use of CLI if used incorrectly
 * @param cmd - The command the user entered
 */
static void
runtime_usage(char *cmd)
{
	debuglog("%s <modules_file>\n", cmd);
}

/**
 * Sets the process data segment (RLIMIT_DATA) and # file descriptors
 * (RLIMIT_NOFILE) soft limit to its hard limit (see man getrlimit)
 */
void
runtime_set_resource_limits_to_max()
{
	struct rlimit resource_limit;
	if (getrlimit(RLIMIT_DATA, &resource_limit) < 0) {
		perror("getrlimit RLIMIT_DATA");
		exit(-1);
	}
	resource_limit.rlim_cur = resource_limit.rlim_max;
	if (setrlimit(RLIMIT_DATA, &resource_limit) < 0) {
		perror("setrlimit RLIMIT_DATA");
		exit(-1);
	}
	if (getrlimit(RLIMIT_NOFILE, &resource_limit) < 0) {
		perror("getrlimit RLIMIT_NOFILE");
		exit(-1);
	}
	resource_limit.rlim_cur = resource_limit.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &resource_limit) < 0) {
		perror("setrlimit RLIMIT_NOFILE");
		exit(-1);
	}
}

/**
 * Check the number of cores and the compiler flags and allocate available cores
 */
void
runtime_allocate_available_cores()
{
	/* Find the number of processors currently online */
	runtime_total_online_processors = sysconf(_SC_NPROCESSORS_ONLN);

	if (runtime_total_online_processors < 2) panic("Runtime requires at least two cores!");

	runtime_first_worker_processor = 1;
	/* WORKER_THREAD_CORE_COUNT can be used as a cap on the number of cores to use, but if there are few
	 * cores that WORKER_THREAD_CORE_COUNT, just use what is available */
	uint32_t max_possible_workers   = runtime_total_online_processors - 1;
	runtime_total_worker_processors = max_possible_workers;
	if (max_possible_workers >= WORKER_THREAD_CORE_COUNT)
		runtime_total_worker_processors = WORKER_THREAD_CORE_COUNT;

	assert(runtime_total_worker_processors == WORKER_THREAD_CORE_COUNT);

	printf("Number of cores %u, sandboxing cores %u (start: %u) and module reqs %u\n",
	       runtime_total_online_processors, runtime_total_worker_processors, runtime_first_worker_processor,
	       LISTENER_THREAD_CORE_ID);
}

/**
 * Returns a float of the cpu MHz entry for CPU0 in /proc/cpuinfo
 * We are assuming all cores are the same clock speed, which is not true of many systems
 * We are also assuming this value is static
 * @return proceccor speed in MHz
 */
static inline float
runtime_get_processor_speed_MHz(void)
{
	float return_value;

	FILE *cmd = popen("grep '^cpu MHz' /proc/cpuinfo | head -n 1 | awk '{print $4}'", "r");
	if (cmd == NULL) goto err;

	float  processor_speed_MHz;
	size_t n;
	char   buff[16];

	if ((n = fread(buff, 1, sizeof(buff) - 1, cmd)) <= 0) goto err;

	buff[n] = '\0';
	if (sscanf(buff, "%f", &processor_speed_MHz) != 1) goto err;

	return_value = processor_speed_MHz;

done:
	pclose(cmd);
	return return_value;
err:
	return_value = -1;
	goto done;
}

#ifdef DEBUG
/**
 * Controls the behavior of the debuglog macro defined in types.h
 * If LOG_TO_FILE is defined, close stdin, stdout, stderr, and debuglog writes to a logfile named awesome.log.
 * Otherwise, it writes to STDOUT
 */
void
runtime_process_debug_log_behavior()
{
#ifdef LOG_TO_FILE
	fclose(stdout);
	fclose(stderr);
	fclose(stdin);
	runtime_log_file_descriptor = open(RUNTIME_LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU | S_IRWXG);
	if (runtime_log_file_descriptor < 0) {
		perror("open");
		exit(-1);
	}
#else
	runtime_log_file_descriptor = 1;
#endif /* LOG_TO_FILE */
}
#endif /* DEBUG */

/**
 * Starts all worker threads and sleeps forever on pthread_join, which should never return
 */
void
runtime_start_runtime_worker_threads()
{
	for (int i = 0; i < runtime_total_worker_processors; i++) {
		int ret = pthread_create(&runtime_worker_threads[i], NULL, worker_thread_main,
		                         (void *)&runtime_worker_threads_argument[i]);
		if (ret) {
			errno = ret;
			perror("pthread_create");
			exit(-1);
		}

		cpu_set_t cs;
		CPU_ZERO(&cs);
		CPU_SET(runtime_first_worker_processor + i, &cs);
		ret = pthread_setaffinity_np(runtime_worker_threads[i], sizeof(cs), &cs);
		assert(ret == 0);
	}
	debuglog("Sandboxing environment ready!\n");

	for (int i = 0; i < runtime_total_worker_processors; i++) {
		int ret = pthread_join(runtime_worker_threads[i], NULL);
		if (ret) {
			errno = ret;
			perror("pthread_join");
			exit(-1);
		}
	}

	exit(-1);
}

int
main(int argc, char **argv)
{
#ifdef DEBUG
	runtime_process_debug_log_behavior();
#endif

#if NCORES == 1
	panic("Runtime requires at least two cores!");
#endif

	debuglog("Initializing the runtime\n");
	if (argc != 2) {
		runtime_usage(argv[0]);
		exit(-1);
	}

	memset(runtime_worker_threads, 0, sizeof(pthread_t) * WORKER_THREAD_CORE_COUNT);

	runtime_processor_speed_MHz                    = runtime_get_processor_speed_MHz();
	software_interrupt_interval_duration_in_cycles = (uint64_t)SOFTWARE_INTERRUPT_INTERVAL_DURATION_IN_USEC
	                                                 * runtime_processor_speed_MHz;
	debuglog("Detected processor speed of %f MHz\n", runtime_processor_speed_MHz);

	runtime_set_resource_limits_to_max();
	runtime_allocate_available_cores();
	runtime_initialize();

	debuglog("Parsing modules file [%s]\n", argv[1]);
	if (module_new_from_json(argv[1])) panic("failed to parse modules file[%s]\n", argv[1]);

	debuglog("Starting listener thread\n");
	listener_thread_initialize();
	debuglog("Starting worker threads\n");
	runtime_start_runtime_worker_threads();
}
