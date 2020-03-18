#include <ctype.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include <module.h>
#include <runtime.h>
#include <sandbox.h>
#include <software_interrupt.h>

// Conditionally used by debuglog when DEBUG is set
#ifdef DEBUG
i32 runtime__log_file_descriptor = -1;
#endif

u32 runtime__total_online_processors                                     = 0;
u32 runtime__total_worker_processors                                     = 0;
u32 runtime__first_worker_processor                                      = 0;
int runtime__runtime__worker_threads_argument[WORKER_THREAD__CORE_COUNT] = {
	0
}; // The worker sets its argument to -1 on error
pthread_t runtime__worker_threads[WORKER_THREAD__CORE_COUNT];


/**
 * Returns instructions on use of CLI if used incorrectly
 * @param cmd - The command the user entered
 **/
static void
runtime__usage(char *cmd)
{
	printf("%s <modules_file>\n", cmd);
	debuglog("%s <modules_file>\n", cmd);
}

/**
 * Sets the process data segment (RLIMIT_DATA) and # file descriptors
 * (RLIMIT_NOFILE) soft limit to its hard limit (see man getrlimit)
 **/
void
runtime__set_resource_limits_to_max()
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
 **/
void
runtime__allocate_available_cores()
{
	// Find the number of processors currently online
	runtime__total_online_processors = sysconf(_SC_NPROCESSORS_ONLN);

	// If multicore, we'll pin one core as a listener and run sandbox threads on all others
	if (runtime__total_online_processors > 1) {
		runtime__first_worker_processor = 1;
		// WORKER_THREAD__CORE_COUNT can be used as a cap on the number of cores to use
		// But if there are few cores that WORKER_THREAD__CORE_COUNT, just use what is available
		u32 max_possible_workers         = runtime__total_online_processors - 1;
		runtime__total_worker_processors = (max_possible_workers >= WORKER_THREAD__CORE_COUNT)
		                                     ? WORKER_THREAD__CORE_COUNT
		                                     : max_possible_workers;
	} else {
		// If single core, we'll do everything on CPUID 0
		runtime__first_worker_processor  = 0;
		runtime__total_worker_processors = 1;
	}
	debuglog("Number of cores %u, sandboxing cores %u (start: %u) and module reqs %u\n",
	         runtime__total_online_processors, runtime__total_worker_processors, runtime__first_worker_processor,
	         LISTENER_THREAD__CORE_ID);
}

#ifdef DEBUG
/**
 * Controls the behavior of the debuglog macro defined in types.h
 * If LOG_TO_FILE is defined, close stdin, stdout, stderr, and debuglog writes to a logfile named awesome.log.
 * Otherwise, it writes to STDOUT
 **/
void
runtime__process_debug_log_behavior()
{
#ifdef LOG_TO_FILE
	fclose(stdout);
	fclose(stderr);
	fclose(stdin);
	runtime__log_file_descriptor = open(RUNTIME__LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU | S_IRWXG);
	if (runtime__log_file_descriptor < 0) {
		perror("open");
		exit(-1);
	}
#else
	runtime__log_file_descriptor = 1;
#endif // LOG_TO_FILE
}
#endif // DEBUG

/**
 * Starts all worker threads and sleeps forever on pthread_join, which should never return
 **/
void
runtime__start_runtime__worker_threads()
{
	for (int i = 0; i < runtime__total_worker_processors; i++) {
		int ret = pthread_create(&runtime__worker_threads[i], NULL, worker_thread__main,
		                         (void *)&runtime__runtime__worker_threads_argument[i]);
		if (ret) {
			errno = ret;
			perror("pthread_create");
			exit(-1);
		}

		cpu_set_t cs;
		CPU_ZERO(&cs);
		CPU_SET(runtime__first_worker_processor + i, &cs);
		ret = pthread_setaffinity_np(runtime__worker_threads[i], sizeof(cs), &cs);
		assert(ret == 0);
	}
	debuglog("Sandboxing environment ready!\n");

	for (int i = 0; i < runtime__total_worker_processors; i++) {
		int ret = pthread_join(runtime__worker_threads[i], NULL);
		if (ret) {
			errno = ret;
			perror("pthread_join");
			exit(-1);
		}
	}

	printf("\nWorker Threads unexpectedly returned!!\n");
	exit(-1);
}

int
main(int argc, char **argv)
{
#ifdef DEBUG
	runtime__process_debug_log_behavior();
#endif

	printf("Starting Awsm\n");
	if (argc != 2) {
		runtime__usage(argv[0]);
		exit(-1);
	}

	memset(runtime__worker_threads, 0, sizeof(pthread_t) * WORKER_THREAD__CORE_COUNT);

	runtime__set_resource_limits_to_max();
	runtime__allocate_available_cores();
	runtime__initialize();

	debuglog("Parsing modules file [%s]\n", argv[1]);
	if (module__new_from_json(argv[1])) {
		printf("failed to parse modules file[%s]\n", argv[1]);
		exit(-1);
	}

	listener_thread__initialize();
	runtime__start_runtime__worker_threads();
}
