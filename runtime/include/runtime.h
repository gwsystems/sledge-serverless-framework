#ifndef SFRT_RUNTIME_H
#define SFRT_RUNTIME_H

#include <sys/epoll.h> // for epoll_create1(), epoll_ctl(), struct epoll_event
#include <uv.h>

#include "module.h"
#include "sandbox.h"
#include "types.h"

extern int                runtime_epoll_file_descriptor;
extern __thread uv_loop_t worker_thread_uvio_handle;
extern float              runtime_processor_speed_MHz;

void         alloc_linear_memory(void);
void         expand_memory(void);
void         free_linear_memory(void *base, u32 bound, u32 max);
INLINE char *get_function_from_table(u32 idx, u32 type_id);
INLINE char *get_memory_ptr_for_runtime(u32 offset, u32 bounds_check);
void         runtime_initialize(void);
void         listener_thread_initialize(void);
void         stub_init(i32 offset);
void *       worker_thread_main(void *return_code);

/**
 * Translates WASM offsets into runtime VM pointers
 * @param offset an offset into the WebAssembly linear memory
 * @param bounds_check the size of the thing we are pointing to
 * @return void pointer to something in WebAssembly linear memory
 **/
static inline void *
worker_thread_get_memory_ptr_void(u32 offset, u32 bounds_check)
{
	return (void *)get_memory_ptr_for_runtime(offset, bounds_check);
}

/**
 * Get a single-byte extended ASCII character from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return char at the offset
 **/
static inline char
worker_thread_get_memory_character(u32 offset)
{
	return get_memory_ptr_for_runtime(offset, 1)[0];
}

/**
 * Get a null-terminated String from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @param max_length the maximum expected length in characters
 * @return pointer to the string or NULL if max_length is reached without finding null-terminator
 **/
static inline char *
worker_thread_get_memory_string(u32 offset, u32 max_length)
{
	for (int i = 0; i < max_length; i++) {
		if (worker_thread_get_memory_character(offset + i) == '\0')
			return worker_thread_get_memory_ptr_void(offset, 1);
	}
	return NULL;
}

/**
 * Get global libuv handle
 **/
static inline uv_loop_t *
worker_thread_get_libuv_handle(void)
{
	return &worker_thread_uvio_handle;
}

unsigned long long __getcycles(void);

#endif /* SFRT_RUNTIME_H */
