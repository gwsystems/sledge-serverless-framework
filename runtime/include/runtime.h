#pragma once

#include <pthread.h>
#include <sys/epoll.h> /* for epoll_create1(), epoll_ctl(), struct epoll_event */
#include <stdatomic.h>
#include <stdbool.h>

#include "likely.h"
#include "types.h"

/* Dedicated Listener Core */
#define LISTENER_THREAD_CORE_ID          0
#define LISTENER_THREAD_MAX_EPOLL_EVENTS 128

#define RUNTIME_LOG_FILE "sledge.log"
/* random! */
#define RUNTIME_MAX_SANDBOX_REQUEST_COUNT (1 << 19)
#define RUNTIME_READ_WRITE_VECTOR_LENGTH  16

/* One Hour. Fits in a uint32_t or an int64_t */
#define RUNTIME_RELATIVE_DEADLINE_US_MAX 3600000000

/* One Hour. Fits in a uint32_t or an int64_t */
#define RUNTIME_EXPECTED_EXECUTION_US_MAX 3600000000

/* 100 MB */
#define RUNTIME_HTTP_REQUEST_SIZE_MAX 100000000
/* 100 MB */
#define RUNTIME_HTTP_RESPONSE_SIZE_MAX 100000000

/* Static buffer used for global deadline array */
#define RUNTIME_MAX_WORKER_COUNT 32

#ifndef NCORES
#warning "NCORES not defined in Makefile. Defaulting to 2"
#define NCORES 2
#endif

#if NCORES == 1
#error "RUNTIME MINIMUM REQUIREMENT IS 2 CORES"
#endif

#define RUNTIME_WORKER_THREAD_CORE_COUNT (NCORES > 1 ? NCORES - 1 : NCORES)

/*
 * Descriptor of the epoll instance used to monitor the socket descriptors of registered
 * serverless modules. The listener cores listens for incoming client requests through this.
 */
extern int runtime_epoll_file_descriptor;

extern int runtime_worker_threads_argument[RUNTIME_WORKER_THREAD_CORE_COUNT];

extern uint64_t runtime_worker_threads_deadline[RUNTIME_WORKER_THREAD_CORE_COUNT];

/* Optional path to a file to log sandbox perf metrics */
extern FILE *runtime_sandbox_perf_log;

/*
 * Assumption: All cores are the same speed
 * See runtime_get_processor_speed_MHz for further details
 */
extern uint32_t runtime_processor_speed_MHz;

/* Count of worker threads and array of their pthread identifiers */
extern pthread_t runtime_worker_threads[];
extern uint32_t  runtime_worker_threads_count;

void         alloc_linear_memory(void);
void         expand_memory(void);
INLINE char *get_function_from_table(uint32_t idx, uint32_t type_id);
INLINE char *get_memory_ptr_for_runtime(uint32_t offset, uint32_t bounds_check);
void         runtime_initialize(void);
void         listener_thread_initialize(void);
void         stub_init(int32_t offset);

unsigned long long __getcycles(void);

/**
 * Used to determine if running in the context of a worker thread
 * @returns true if worker. false if listener core
 */
static inline bool
runtime_is_worker()
{
	pthread_t self = pthread_self();
	for (int i = 0; i < runtime_worker_threads_count; i++) {
		if (runtime_worker_threads[i] == self) return true;
	}

	return false;
}

enum RUNTIME_SCHEDULER
{
	RUNTIME_SCHEDULER_FIFO = 0,
	RUNTIME_SCHEDULER_EDF  = 1
};

static inline char *
print_runtime_scheduler(enum RUNTIME_SCHEDULER variant)
{
	switch (variant) {
	case RUNTIME_SCHEDULER_FIFO:
		return "FIFO";
	case RUNTIME_SCHEDULER_EDF:
		return "EDF";
	}
};

enum RUNTIME_SIGALRM_HANDLER
{
	RUNTIME_SIGALRM_HANDLER_BROADCAST = 0,
	RUNTIME_SIGALRM_HANDLER_TRIAGED   = 1
};


static inline char *
print_runtime_sigalrm_handler(enum RUNTIME_SIGALRM_HANDLER variant)
{
	switch (variant) {
	case RUNTIME_SIGALRM_HANDLER_BROADCAST:
		return "BROADCAST";
	case RUNTIME_SIGALRM_HANDLER_TRIAGED:
		return "TRIAGED";
	}
};

extern enum RUNTIME_SCHEDULER       runtime_scheduler;
extern enum RUNTIME_SIGALRM_HANDLER runtime_sigalrm_handler;
extern bool                         runtime_preemption_enabled;
extern uint32_t                     runtime_quantum_us;
