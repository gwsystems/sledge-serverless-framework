#pragma once

#include <pthread.h>
#include <sys/epoll.h> /* for epoll_create1(), epoll_ctl(), struct epoll_event */
#include <stdbool.h>

#include "types.h"

#define LISTENER_THREAD_CORE_ID          0 /* Dedicated Listener Core */
#define LISTENER_THREAD_MAX_EPOLL_EVENTS 1024

#define RUNTIME_LOG_FILE                  "awesome.log"
#define RUNTIME_MAX_SANDBOX_REQUEST_COUNT (1 << 19) /* random! */
#define RUNTIME_READ_WRITE_VECTOR_LENGTH  16

/*
 * Descriptor of the epoll instance used to monitor the socket descriptors of registered
 * serverless modules. The listener cores listens for incoming client requests through this.
 */
extern int runtime_epoll_file_descriptor;

/*
 * Assumption: All cores are the same speed
 * See runtime_get_processor_speed_MHz for further details
 */
extern float runtime_processor_speed_MHz;

/* Count of worker threads and array of their pthread identifiers */
extern pthread_t runtime_worker_threads[];
extern uint32_t  runtime_worker_threads_count;

/*
 * Unitless estimate of the instantaneous fraction of system capacity required to complete all previously
 * admitted work. This is used to calculate free capacity as part of admissions control
 *
 * The estimated requirements of a single admitted request is calculated as
 * estimated execution time (cycles) / relative deadline (cycles)
 *
 * These estimates are incremented on request acceptance and decremented on request completion (either
 * success or failure)
 */
extern double runtime_admitted;

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
