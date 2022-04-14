#include <ctype.h>
#include <dlfcn.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef LOG_TO_FILE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#endif

#include "pretty_print.h"
#include "debuglog.h"
#include "listener_thread.h"
#include "module.h"
#include "panic.h"
#include "runtime.h"
#include "sandbox_types.h"
#include "scheduler.h"
#include "software_interrupt.h"
#include "worker_thread.h"

/* Conditionally used by debuglog when NDEBUG is not set */
int32_t  debuglog_file_descriptor        = -1;
uint32_t runtime_first_worker_processor  = 1;
uint32_t runtime_processor_speed_MHz     = 0;
uint32_t runtime_total_online_processors = 0;
uint32_t runtime_worker_threads_count    = 0;

enum RUNTIME_SIGALRM_HANDLER runtime_sigalrm_handler = RUNTIME_SIGALRM_HANDLER_BROADCAST;
int                          runtime_worker_core_count;


bool     runtime_preemption_enabled = true;
uint32_t runtime_quantum_us         = 5000; /* 5ms */

/**
 * Returns instructions on use of CLI if used incorrectly
 * @param cmd - The command the user entered
 */
static void
runtime_usage(char *cmd)
{
	printf("%s <modules_file>\n", cmd);
}

/**
 * Check the number of cores and the compiler flags and allocate available cores
 */
void
runtime_allocate_available_cores()
{
	uint32_t max_possible_workers;

	/* Find the number of processors currently online */
	runtime_total_online_processors = sysconf(_SC_NPROCESSORS_ONLN);

	pretty_print_key_value("Core Count (Online)", "%u\n", runtime_total_online_processors);

	/* If more than two cores are available, leave core 0 free to run OS tasks */
	if (runtime_total_online_processors > 2) {
		runtime_first_worker_processor = 2;
		max_possible_workers           = runtime_total_online_processors - 2;
	} else if (runtime_total_online_processors == 2) {
		runtime_first_worker_processor = 1;
		max_possible_workers           = runtime_total_online_processors - 1;
	} else {
		panic("Runtime requires at least two cores!");
	}


	/* Number of Workers */
	char *worker_count_raw = getenv("SLEDGE_NWORKERS");
	if (worker_count_raw != NULL) {
		int worker_count = atoi(worker_count_raw);
		if (worker_count <= 0 || worker_count > max_possible_workers) {
			panic("Invalid Worker Count. Was %d. Must be {1..%d}\n", worker_count, max_possible_workers);
		}
		runtime_worker_threads_count = worker_count;
	} else {
		runtime_worker_threads_count = max_possible_workers;
	}

	pretty_print_key_value("Listener core ID", "%u\n", LISTENER_THREAD_CORE_ID);
	pretty_print_key_value("First Worker core ID", "%u\n", runtime_first_worker_processor);
	pretty_print_key_value("Worker core count", "%u\n", runtime_worker_threads_count);
}

/**
 * Returns the cpu MHz entry for CPU0 in /proc/cpuinfo, rounded to the nearest MHz
 * We are assuming all cores are the same clock speed, which is not true of many systems
 * We are also assuming this value is static
 * @return proceccor speed in MHz
 */
static inline uint32_t
runtime_get_processor_speed_MHz(void)
{
	uint32_t return_value;

	FILE *cmd = popen("grep '^cpu MHz' /proc/cpuinfo | head -n 1 | awk '{print $4}'", "r");
	if (unlikely(cmd == NULL)) goto err;

	char   buff[16];
	size_t n = fread(buff, 1, sizeof(buff) - 1, cmd);
	if (unlikely(n <= 0)) goto err;
	buff[n] = '\0';

	float processor_speed_MHz;
	n = sscanf(buff, "%f", &processor_speed_MHz);
	if (unlikely(n != 1)) goto err;
	if (unlikely(processor_speed_MHz < 0)) goto err;

	return_value = (uint32_t)nearbyintf(processor_speed_MHz);

done:
	pclose(cmd);
	return return_value;
err:
	return_value = 0;
	goto done;
}

/**
 * Controls the behavior of the debuglog macro defined in types.h
 * If LOG_TO_FILE is defined, close stdin, stdout, stderr, and debuglog writes to a logfile named awesome.log.
 * Otherwise, it writes to STDOUT
 */
void
runtime_process_debug_log_behavior()
{
#ifdef LOG_TO_FILE
	debuglog_file_descriptor = open(RUNTIME_LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU | S_IRWXG);
	if (debuglog_file_descriptor < 0) {
		perror("Error opening logfile\n");
		exit(-1);
	}
	dup2(debuglog_file_descriptor, STDOUT_FILENO);
	dup2(debuglog_file_descriptor, STDERR_FILENO);
#else
	debuglog_file_descriptor = STDOUT_FILENO;
#endif /* LOG_TO_FILE */
}

/**
 * Starts all worker threads and sleeps forever on pthread_join, which should never return
 */
void
runtime_start_runtime_worker_threads()
{
	printf("Starting %d worker thread(s)\n", runtime_worker_threads_count);
	for (int i = 0; i < runtime_worker_threads_count; i++) {
		/* Pass the value we want the threads to use when indexing into global arrays of per-thread values */
		runtime_worker_threads_argument[i] = i;
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
}

void
runtime_configure()
{
	/* Scheduler Policy */
	char *scheduler_policy = getenv("SLEDGE_SCHEDULER");
	if (scheduler_policy == NULL) scheduler_policy = "EDF";
	if (strcmp(scheduler_policy, "EDF") == 0) {
		scheduler = SCHEDULER_EDF;
	} else if (strcmp(scheduler_policy, "FIFO") == 0) {
		scheduler = SCHEDULER_FIFO;
	} else {
		panic("Invalid scheduler policy: %s. Must be {EDF|FIFO}\n", scheduler_policy);
	}
	pretty_print_key_value("Scheduler Policy", "%s\n", scheduler_print(scheduler));

	/* Sigalrm Handler Technique */
	char *sigalrm_policy = getenv("SLEDGE_SIGALRM_HANDLER");
	if (sigalrm_policy == NULL) sigalrm_policy = "BROADCAST";
	if (strcmp(sigalrm_policy, "BROADCAST") == 0) {
		runtime_sigalrm_handler = RUNTIME_SIGALRM_HANDLER_BROADCAST;
	} else if (strcmp(sigalrm_policy, "TRIAGED") == 0) {
		if (unlikely(scheduler != SCHEDULER_EDF)) panic("triaged sigalrm handlers are only valid with EDF\n");
		runtime_sigalrm_handler = RUNTIME_SIGALRM_HANDLER_TRIAGED;
	} else {
		panic("Invalid sigalrm policy: %s. Must be {BROADCAST|TRIAGED}\n", sigalrm_policy);
	}
	pretty_print_key_value("Sigalrm Policy", "%s\n", runtime_print_sigalrm_handler(runtime_sigalrm_handler));

	/* Runtime Preemption Toggle */
	char *preempt_disable = getenv("SLEDGE_DISABLE_PREEMPTION");
	if (preempt_disable != NULL && strcmp(preempt_disable, "false") != 0) runtime_preemption_enabled = false;
	pretty_print_key_value("Preemption", "%s\n",
	                       runtime_preemption_enabled ? PRETTY_PRINT_GREEN_ENABLED : PRETTY_PRINT_RED_DISABLED);

	/* Runtime Quantum */
	char *quantum_raw = getenv("SLEDGE_QUANTUM_US");
	if (quantum_raw != NULL) {
		long quantum = atoi(quantum_raw);
		if (unlikely(quantum <= 0)) panic("SLEDGE_QUANTUM_US must be a positive integer, saw %ld\n", quantum);
		if (unlikely(quantum > 999999))
			panic("SLEDGE_QUANTUM_US must be less than 999999 ms, saw %ld\n", quantum);
		runtime_quantum_us = (uint32_t)quantum;
	}
	pretty_print_key_value("Quantum", "%u us\n", runtime_quantum_us);

	sandbox_perf_log_init();
}

void
log_compiletime_config()
{
	/* System Stuff */
	printf("System Flags:\n");

	pretty_print_key("Architecture");
#if defined(aarch64)
	printf("aarch64\n");
#elif defined(x86_64)
	printf("x86_64\n");
#endif

	pretty_print_key_value("Page Size", "%lu\n", PAGE_SIZE);

	/* Feature Toggles */
	printf("Static Compiler Flags (Features):\n");

#ifdef ADMISSIONS_CONTROL
	pretty_print_key_enabled("Admissions Control");
#else
	pretty_print_key_disabled("Admissions Control");
#endif

	/* Debugging Flags */
	printf("Static Compiler Flags (Debugging):\n");

#ifndef NDEBUG
	pretty_print_key_enabled("Assertions and Debug Logs");
#else
	pretty_print_key_disabled("Assertions and Debug Logs");
#endif

	pretty_print_key("Logging to");
#ifdef LOG_TO_FILE
	printf("%s\n", RUNTIME_LOG_FILE);
#else
	printf("STDOUT and STDERR\n");
#endif

#ifdef LOG_ADMISSIONS_CONTROL
	pretty_print_key_enabled("Log Admissions Control");
#else
	pretty_print_key_disabled("Log Admissions Control");
#endif

#ifdef LOG_CONTEXT_SWITCHES
	pretty_print_key_enabled("Log Context Switches");
#else
	pretty_print_key_disabled("Log Context Switches");
#endif

#ifdef LOG_HTTP_PARSER
	pretty_print_key_enabled("Log HTTP Parser");
#else
	pretty_print_key_disabled("Log HTTP Parser");
#endif

#ifdef LOG_LOCK_OVERHEAD
	pretty_print_key_enabled("Log Lock Overhead");
#else
	pretty_print_key_disabled("Log Lock Overhead");
#endif

#ifdef LOG_MODULE_LOADING
	pretty_print_key_enabled("Log Module Loading");
#else
	pretty_print_key_disabled("Log Module Loading");
#endif

#ifdef LOG_PREEMPTION
	pretty_print_key_enabled("Log Preemption");
#else
	pretty_print_key_disabled("Log Preemption");
#endif

#ifdef LOG_SANDBOX_ALLOCATION
	pretty_print_key_enabled("Log Request Allocation");
#else
	pretty_print_key_disabled("Log Request Allocation");
#endif

#ifdef LOG_SOFTWARE_INTERRUPT_COUNTS
	pretty_print_key_enabled("Log Software Interrupt Counts");
#else
	pretty_print_key_disabled("Log Software Interrupt Counts");
#endif

#ifdef LOG_STATE_CHANGES
	pretty_print_key_enabled("Log State Changes");
#else
	pretty_print_key_disabled("Log State Changes");
#endif

#ifdef LOG_TOTAL_REQS_RESPS
	pretty_print_key_enabled("Log Total Reqs/Resps");
#else
	pretty_print_key_disabled("Log Total Reqs/Resps");
#endif

#ifdef SANDBOX_STATE_TOTALS
	pretty_print_key_enabled("Log Sandbox State Count");
#else
	pretty_print_key_disabled("Log Sandbox State Count");
#endif

#ifdef LOG_LOCAL_RUNQUEUE
	pretty_print_key_enabled("Log Local Runqueue");
#else
	pretty_print_key_disabled("Log Local Runqueue");
#endif
}

void
check_versions()
{
	// Additional functions have become async signal safe over time. Validate latest
	static_assert(_POSIX_VERSION >= 200809L, "Requires POSIX 2008 or higher\n");
	// We use C18 features
	static_assert(__STDC_VERSION__ >= 201710, "Requires C18 or higher\n");
	static_assert(__linux__ == 1, "Requires epoll, a Linux-only feature");
}

/**
 * Allocates a buffer in memory containing the entire contents of the file provided
 * @param file_name file to load into memory
 * @param ret_ptr Pointer to set with address of buffer this function allocates. The caller must free this!
 * @return size of the allocated buffer or -1 in case of error;
 */
static inline size_t
load_file_into_buffer(const char *file_name, char **file_buffer)
{
	/* Use stat to get file attributes and make sure file is present and not empty */
	struct stat stat_buffer;
	if (stat(file_name, &stat_buffer) < 0) {
		fprintf(stderr, "Attempt to stat %s failed: %s\n", file_name, strerror(errno));
		goto err;
	}
	if (stat_buffer.st_size == 0) {
		fprintf(stderr, "File %s is unexpectedly empty\n", file_name);
		goto err;
	}
	if (!S_ISREG(stat_buffer.st_mode)) {
		fprintf(stderr, "File %s is not a regular file\n", file_name);
		goto err;
	}

	/* Open the file */
	FILE *module_file = fopen(file_name, "r");
	if (!module_file) {
		fprintf(stderr, "Attempt to open %s failed: %s\n", file_name, strerror(errno));
		goto err;
	}

	/* Initialize a Buffer */
	*file_buffer = calloc(1, stat_buffer.st_size);
	if (*file_buffer == NULL) {
		fprintf(stderr, "Attempt to allocate file buffer failed: %s\n", strerror(errno));
		goto stat_buffer_alloc_err;
	}

	/* Read the file into the buffer and check that the buffer size equals the file size */
	ssize_t total_chars_read = fread(*file_buffer, sizeof(char), stat_buffer.st_size, module_file);
#ifdef LOG_MODULE_LOADING
	debuglog("size read: %d content: %s\n", total_chars_read, *file_buffer);
#endif
	if (total_chars_read != stat_buffer.st_size) {
		fprintf(stderr, "Attempt to read %s into buffer failed: %s\n", file_name, strerror(errno));
		goto fread_err;
	}
	assert(total_chars_read > 0);

	/* Close the file */
	if (fclose(module_file) == EOF) {
		fprintf(stderr, "Attempt to close buffer containing %s failed: %s\n", file_name, strerror(errno));
		goto fclose_err;
	};
	module_file = NULL;

	return total_chars_read;

fclose_err:
	/* We will retry fclose when we fall through into stat_buffer_alloc_err */
fread_err:
	free(*file_buffer);
stat_buffer_alloc_err:
	// Check to ensure we haven't already close this
	if (module_file != NULL) {
		if (fclose(module_file) == EOF) panic("Failed to close file\n");
	}
err:
	return (ssize_t)-1;
}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		runtime_usage(argv[0]);
		exit(-1);
	}

	printf("Starting the Sledge runtime\n");


	log_compiletime_config();
	runtime_process_debug_log_behavior();

	printf("Runtime Environment:\n");

	runtime_processor_speed_MHz = runtime_get_processor_speed_MHz();
	if (unlikely(runtime_processor_speed_MHz == 0)) panic("Failed to detect processor speed\n");

	int heading_length = 30;

	pretty_print_key_value("Processor Speed", "%u MHz\n", runtime_processor_speed_MHz);

	runtime_set_resource_limits_to_max();
	runtime_allocate_available_cores();
	runtime_configure();
	runtime_initialize();
	software_interrupt_initialize();

	listener_thread_initialize();
	runtime_start_runtime_worker_threads();
	software_interrupt_arm_timer();

#ifdef LOG_MODULE_LOADING
	debuglog("Parsing modules file [%s]\n", argv[1]);
#endif
	const char *json_path    = argv[1];
	char       *json_buf     = NULL;
	ssize_t     json_buf_len = load_file_into_buffer(json_path, &json_buf);
	if (unlikely(json_buf_len <= 0)) panic("failed to initialize module(s) defined in %s\n", json_path);
	int rc = module_alloc_from_json(json_buf, json_buf_len);
	if (unlikely(rc != 0)) panic("failed to initialize module(s) defined in %s\n", json_path);

	for (int i = 0; i < runtime_worker_threads_count; i++) {
		int ret = pthread_join(runtime_worker_threads[i], NULL);
		if (ret) {
			errno = ret;
			perror("pthread_join");
			exit(-1);
		}
	}

	exit(-1);
}
