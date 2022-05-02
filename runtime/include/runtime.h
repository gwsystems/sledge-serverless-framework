#pragma once

#include <pthread.h>
#include <sys/epoll.h> /* for epoll_create1(), epoll_ctl(), struct epoll_event */
#include <stdatomic.h>
#include <stdbool.h>

#include "likely.h"
#include "types.h"

/**
 * Optimizing compilers and modern CPUs reorder instructions however it sees fit. This means that the resulting
 * execution order may differ from the order of our source code. If there is a variable protecting a critical section,
 * this means that code may move out of or into the critical section, which could cause bugs. In order to protect
 * against this, we need to improve an ordering contraint via a "memory barrier." Inline assembly acts as a such barrier
 * that no assembly instructions can be reordered across. An example of how this is used in this code base is in ensure
 * that code is either intentionally preemptable or non-preemptable.
 *
 * Wikipedia: https://en.wikipedia.org/wiki/Memory_barrier
 * Linux: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/Documentation/memory-barriers.txt
 */
#define barrier() __asm__ __volatile__("" ::: "memory")

#define RUNTIME_EXPECTED_EXECUTION_US_MAX 3600000000
#define RUNTIME_HTTP_REQUEST_SIZE_MAX     100000000 /* 100 MB */
#define RUNTIME_HTTP_RESPONSE_SIZE_MAX    100000000 /* 100 MB */
#define RUNTIME_LOG_FILE                  "sledge.log"
#define RUNTIME_MAX_EPOLL_EVENTS          128
#define RUNTIME_MAX_WORKER_COUNT          32 /* Static buffer size for per-worker globals */
#define RUNTIME_READ_WRITE_VECTOR_LENGTH  16
#define RUNTIME_RELATIVE_DEADLINE_US_MAX  3600000000 /* One Hour. Fits in uint32_t */
#define RUNTIME_REPLENISH_PERIOD_US_MAX   3600000000
#define RUNTIME_MAX_BUDGET_US_MAX         3600000000

#define RUNTIME_RUNQUEUE_SIZE     256
#define RUNTIME_MODULE_QUEUE_SIZE 4096 * 100

enum RUNTIME_SIGALRM_HANDLER
{
	RUNTIME_SIGALRM_HANDLER_BROADCAST = 0,
	RUNTIME_SIGALRM_HANDLER_TRIAGED   = 1
};

extern bool                         runtime_preemption_enabled;
extern uint32_t                     runtime_processor_speed_MHz;
extern uint32_t                     runtime_quantum_us;
extern enum RUNTIME_SIGALRM_HANDLER runtime_sigalrm_handler;
extern pthread_t                   *runtime_worker_threads;
extern uint32_t                     runtime_worker_threads_count;
extern int                         *runtime_worker_threads_argument;
extern uint64_t                    *runtime_worker_threads_deadline;
extern uint64_t                     runtime_boot_timestamp;

extern void runtime_initialize(void);
extern void runtime_set_pthread_prio(pthread_t thread, unsigned int nice);
extern void runtime_set_resource_limits_to_max(void);

/* External Symbols */
extern int   expand_memory(void);
INLINE char *get_function_from_table(uint32_t idx, uint32_t type_id);
INLINE char *get_memory_ptr(uint32_t offset, uint32_t length);
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
