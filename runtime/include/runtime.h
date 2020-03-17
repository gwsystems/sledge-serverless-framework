#ifndef SFRT_RUNTIME_H
#define SFRT_RUNTIME_H

#include <sys/epoll.h> // for epoll_create1(), epoll_ctl(), struct epoll_event
#include <uv.h>

#include "module.h"
#include "sandbox.h"
#include "types.h"

extern int                   runtime__epoll_file_descriptor;
extern struct deque_sandbox *runtime__global_deque;
extern pthread_mutex_t       runtime__global_deque_mutex;
extern __thread uv_loop_t    worker_thread__uvio_handle;

void         alloc_linear_memory(void);
void         expand_memory(void);
void         free_linear_memory(void *base, u32 bound, u32 max);
INLINE char *get_function_from_table(u32 idx, u32 type_id);
INLINE char *get_memory_ptr_for_runtime(u32 offset, u32 bounds_check);
void         runtime__initialize(void);
void         listener_thread__initialize(void);
void         stub_init(i32 offset);
void *       worker_thread__main(void *return_code);

/**
 * TODO: ???
 * @param offset TODO: ????
 * @param bounds_check TODO: ???
 * @return TODO: ???
 **/
static inline void *
get_memory_ptr_void(u32 offset, u32 bounds_check)
{
	return (void *)get_memory_ptr_for_runtime(offset, bounds_check);
}

/**
 * TODO: ???
 * @param offset TODO: ????
 * @return TODO: ???
 **/
static inline char *
get_memory_string(u32 offset)
{
	char *naive_ptr = get_memory_ptr_for_runtime(offset, 1);
	int   i         = 0;

	while (true) {
		// Keep bounds checking the waters over and over until we know it's safe (we find a terminating
		// character)
		char ith_element = get_memory_ptr_for_runtime(offset, i + 1)[i];

		if (ith_element == '\0') return naive_ptr;
		i++;
	}
}

/**
 * Get global libuv handle
 **/
static inline uv_loop_t *
get_thread_libuv_handle(void)
{
	return &worker_thread__uvio_handle;
}

#endif /* SFRT_RUNTIME_H */
