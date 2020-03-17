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
#include <softint.h>

i32       log_file_descriptor                  = -1;
u32       total_online_processors              = 0;
u32       total_worker_processors              = 0;
u32       first_worker_processor               = 0;
int       worker_threads_argument[SBOX_NCORES] = { 0 }; // The worker sets its argument to -1 on error
pthread_t worker_threads[SBOX_NCORES];


/**
 * Returns instructions on use of CLI if used incorrectly
 * @param cmd - The command the user entered
 **/
static void
usage(char *cmd)
{
	printf("%s <modules_file>\n", cmd);
	debuglog("%s <modules_file>\n", cmd);
}

/**
 * Sets the process data segment (RLIMIT_DATA) and # file descriptors
 * (RLIMIT_NOFILE) soft limit to its hard limit (see man getrlimit)
 **/
void
set_resource_limits_to_max()
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
allocate_available_cores()
{
	// Find the number of processors currently online
	total_online_processors = sysconf(_SC_NPROCESSORS_ONLN);

	// If multicore, we'll pin one core as a listener and run sandbox threads on all others
	if (total_online_processors > 1) {
		first_worker_processor = 1;
		// SBOX_NCORES can be used as a cap on the number of cores to use
		// But if there are few cores that SBOX_NCORES, just use what is available
		u32 max_possible_workers = total_online_processors - 1;
		total_worker_processors  = (max_possible_workers >= SBOX_NCORES) ? SBOX_NCORES : max_possible_workers;
	} else {
		// If single core, we'll do everything on CPUID 0
		first_worker_processor  = 0;
		total_worker_processors = 1;
	}
	debuglog("Number of cores %u, sandboxing cores %u (start: %u) and module reqs %u\n", total_online_processors,
	         total_worker_processors, first_worker_processor, MOD_REQ_CORE);
}

/**
 * If NOSTIO is defined, close stdin, stdout, stderr, and write to logfile named awesome.log.
 * Otherwise, log to STDOUT
 * NOSTIO = No Standard Input/Output?
 **/
void
process_nostio()
{
#ifdef NOSTDIO
	fclose(stdout);
	fclose(stderr);
	fclose(stdin);
	log_file_descriptor = open(LOGFILE, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU | S_IRWXG);
	if (log_file_descriptor < 0) {
		perror("open");
		exit(-1);
	}
#else
	log_file_descriptor = 1;
#endif
}

/**
 * Starts all worker threads and sleeps forever on pthread_join, which should never return
 **/
void
start_worker_threads()
{
	for (int i = 0; i < total_worker_processors; i++) {
		int ret = pthread_create(&worker_threads[i], NULL, worker_thread_main,
		                         (void *)&worker_threads_argument[i]);
		if (ret) {
			errno = ret;
			perror("pthread_create");
			exit(-1);
		}

		cpu_set_t cs;
		CPU_ZERO(&cs);
		CPU_SET(first_worker_processor + i, &cs);
		ret = pthread_setaffinity_np(worker_threads[i], sizeof(cs), &cs);
		assert(ret == 0);
	}
	debuglog("Sandboxing environment ready!\n");

	for (int i = 0; i < total_worker_processors; i++) {
		int ret = pthread_join(worker_threads[i], NULL);
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
	printf("Starting Awsm\n");
	if (argc != 2) {
		usage(argv[0]);
		exit(-1);
	}

	memset(worker_threads, 0, sizeof(pthread_t) * SBOX_NCORES);

	set_resource_limits_to_max();
	allocate_available_cores();
	process_nostio();
	runtime__initialize();

	debuglog("Parsing modules file [%s]\n", argv[1]);
	if (module__new_from_json(argv[1])) {
		printf("failed to parse modules file[%s]\n", argv[1]);
		exit(-1);
	}

	listener_thread__initialize();
	start_worker_threads();
}
