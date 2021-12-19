#pragma once

#include <pthread.h>
#include <sys/epoll.h> /* for epoll_create1(), epoll_ctl(), struct epoll_event */
#include <stdatomic.h>
#include <stdbool.h>

#include "likely.h"
#include "types.h"

#ifndef NCORES
#warning "NCORES not defined in Makefile. Defaulting to 2"
#define NCORES 2
#endif

#if NCORES == 1
#error "RUNTIME MINIMUM REQUIREMENT IS 2 CORES"
#endif

#define RUNTIME_EXPECTED_EXECUTION_US_MAX 3600000000
#define RUNTIME_HTTP_REQUEST_SIZE_MAX     100000000 /* 100 MB */
#define RUNTIME_HTTP_RESPONSE_SIZE_MAX    100000000 /* 100 MB */
#define RUNTIME_LOG_FILE                  "sledge.log"
#define RUNTIME_MAX_EPOLL_EVENTS          128
#define RUNTIME_MAX_SANDBOX_REQUEST_COUNT (1 << 19)
#define RUNTIME_MAX_WORKER_COUNT          32 /* Static buffer size for per-worker globals */
#define RUNTIME_READ_WRITE_VECTOR_LENGTH  16
#define RUNTIME_RELATIVE_DEADLINE_US_MAX  3600000000 /* One Hour. Fits in uint32_t */
#define RUNTIME_WORKER_THREAD_CORE_COUNT  (NCORES > 1 ? NCORES - 1 : NCORES)

enum RUNTIME_SIGALRM_HANDLER
{
	RUNTIME_SIGALRM_HANDLER_BROADCAST = 0,
	RUNTIME_SIGALRM_HANDLER_TRIAGED   = 1
};

extern bool                         runtime_preemption_enabled;
extern uint32_t                     runtime_processor_speed_MHz;
extern uint32_t                     runtime_quantum_us;
extern FILE *                       runtime_sandbox_perf_log;
extern enum RUNTIME_SIGALRM_HANDLER runtime_sigalrm_handler;
extern pthread_t                    runtime_worker_threads[];
extern uint32_t                     runtime_worker_threads_count;
extern int                          runtime_worker_threads_argument[RUNTIME_WORKER_THREAD_CORE_COUNT];
extern uint64_t                     runtime_worker_threads_deadline[RUNTIME_WORKER_THREAD_CORE_COUNT];
extern uint64_t                     runtime_worker_threads_remaining_slack[RUNTIME_WORKER_THREAD_CORE_COUNT];

extern void runtime_initialize(void);
extern void runtime_set_pthread_prio(pthread_t thread, unsigned int nice);
extern void runtime_set_resource_limits_to_max(void);

/* External Symbols */
extern void  alloc_linear_memory(void);
extern void  expand_memory(void);
INLINE char *get_function_from_table(uint32_t idx, uint32_t type_id);
INLINE char *get_memory_ptr_for_runtime(uint32_t offset, uint32_t bounds_check);
extern void  stub_init(int32_t offset);

static inline char *
runtime_print_sigalrm_handler(enum RUNTIME_SIGALRM_HANDLER variant)
{
	switch (variant) {
	case RUNTIME_SIGALRM_HANDLER_BROADCAST:
		return "BROADCAST";
	case RUNTIME_SIGALRM_HANDLER_TRIAGED:
		return "TRIAGED";
	}
}
