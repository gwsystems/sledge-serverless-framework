#pragma once

#include <uv.h>
#include "types.h"

extern __thread uv_loop_t worker_thread_uvio_handle;

void *worker_thread_main(void *return_code);

/**
 * Translates WASM offsets into runtime VM pointers
 * @param offset an offset into the WebAssembly linear memory
 * @param bounds_check the size of the thing we are pointing to
 * @return void pointer to something in WebAssembly linear memory
 */
static inline void *
worker_thread_get_memory_ptr_void(u32 offset, u32 bounds_check)
{
	return (void *)get_memory_ptr_for_runtime(offset, bounds_check);
}

/**
 * Get a single-byte extended ASCII character from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return char at the offset
 */
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
 */
static inline char *
worker_thread_get_memory_string(u32 offset, u32 max_length)
{
	for (int i = 0; i < max_length; i++) {
		if (worker_thread_get_memory_character(offset + i) == '\0') {
			return worker_thread_get_memory_ptr_void(offset, 1);
		}
	}
	return NULL;
}

/**
 * Get global libuv handle
 */
static inline uv_loop_t *
worker_thread_get_libuv_handle(void)
{
	return &worker_thread_uvio_handle;
}
