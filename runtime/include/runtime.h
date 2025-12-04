#pragma once

#include <pthread.h>
#include <semaphore.h>
#include <sys/epoll.h> /* for epoll_create1(), epoll_ctl(), struct epoll_event */
#include <sys/types.h> /* for pid_t */
#include <stdatomic.h>
#include <stdbool.h>

#include "types.h"
#include "pool.h"
#include "wasm_stack.h"
#include "wasm_memory.h"
#include "likely.h"
#include "types.h"

INIT_POOL(wasm_memory, wasm_memory_free)
INIT_POOL(wasm_stack, wasm_stack_free)

struct memory_pool {
        struct wasm_memory_pool memory;
        struct wasm_stack_pool  stack;
} CACHE_PAD_ALIGNED;


extern struct memory_pool *memory_pools;
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

#define RUNTIME_LOG_FILE                 "sledge.log"
#define RUNTIME_MAX_EPOLL_EVENTS         128
#define RUNTIME_MAX_TENANT_COUNT         65535 /* Use UDP port to index tenent */ 
#define RUNTIME_RELATIVE_DEADLINE_US_MAX 3600000000 /* One Hour. Fits in uint32_t */
#define RUNTIME_RUNQUEUE_SIZE            2560000        /* Minimum guaranteed size. Might grow! */
#define RUNTIME_TENANT_QUEUE_SIZE        4096

enum RUNTIME_SIGALRM_HANDLER
{
	RUNTIME_SIGALRM_HANDLER_BROADCAST = 0,
	RUNTIME_SIGALRM_HANDLER_TRIAGED   = 1
};

extern _Atomic uint64_t request_index;
extern pid_t                        runtime_pid;
extern bool                         runtime_preemption_enabled;
extern bool                         runtime_worker_spinloop_pause_enabled;
extern uint32_t                     runtime_processor_speed_MHz;
extern uint32_t                     runtime_quantum_us;
extern enum RUNTIME_SIGALRM_HANDLER runtime_sigalrm_handler;
extern pthread_t                   *runtime_worker_threads;
extern pthread_t                   *runtime_listener_threads;
extern uint32_t                     runtime_worker_threads_count;
extern uint32_t                     runtime_listener_threads_count;
extern int                         *runtime_worker_threads_argument;
extern int                         *runtime_listener_threads_argument;
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

static inline void
request_index_initialize()
{
        atomic_init(&request_index, 0);
}
static inline uint64_t
request_index_increment()
{
        return atomic_fetch_add(&request_index, 1);
}

static inline void
runtime_initialize_pools()
{
        for (int i = 0; i < runtime_worker_threads_count; i++) {
                wasm_memory_pool_init(&memory_pools[i].memory, false);
                wasm_stack_pool_init(&memory_pools[i].stack, false);
        }
}

static inline void
runtime_deinitialize_pools()
{
        for (int i = 0; i < runtime_worker_threads_count; i++) {
                wasm_memory_pool_deinit(&memory_pools[i].memory);
                wasm_stack_pool_deinit(&memory_pools[i].stack);
        }
}

static inline struct wasm_stack * 
runtime_stack_pool_remove(int thread_id) {
	return wasm_stack_pool_remove_nolock(&memory_pools[thread_id].stack);
}

static inline void
runtime_stack_pool_add(int thread_id, struct wasm_stack * stack) {
	wasm_stack_pool_add_nolock(&memory_pools[thread_id].stack, stack);
}

static inline struct wasm_memory *
runtime_linear_memory_pool_remove(int thread_id) {
	return wasm_memory_pool_remove_nolock(&memory_pools[thread_id].memory);
}

static inline void
runtime_linear_memory_pool_add(int thread_id, struct wasm_memory * memory) {
	wasm_memory_pool_add_nolock(&memory_pools[thread_id].memory, memory);
}
