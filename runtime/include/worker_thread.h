#pragma once

#include "runtime.h"

#ifndef NCORES
#warning "NCORES not defined in Makefile. Defaulting to 2"
#define NCORES 2
#endif

#if NCORES == 1
#error "RUNTIME MINIMUM REQUIREMENT IS 2 CORES"
#endif

#define WORKER_THREAD_CORE_COUNT (NCORES > 1 ? NCORES - 1 : NCORES)

extern __thread uint64_t worker_thread_lock_duration;
extern __thread uint64_t worker_thread_start_timestamp;
extern __thread int      worker_thread_epoll_file_descriptor;

void *worker_thread_main(void *return_code);

/**
 * Translates WASM offsets into runtime VM pointers
 * @param offset an offset into the WebAssembly linear memory
 * @param bounds_check the size of the thing we are pointing to
 * @return void pointer to something in WebAssembly linear memory
 */
static inline void *
worker_thread_get_memory_ptr_void(uint32_t offset, uint32_t bounds_check)
{
	return (void *)get_memory_ptr_for_runtime(offset, bounds_check);
}

/**
 * Get a single-byte extended ASCII character from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return char at the offset
 */
static inline char
worker_thread_get_memory_character(uint32_t offset)
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
worker_thread_get_memory_string(uint32_t offset, uint32_t max_length)
{
	for (int i = 0; i < max_length; i++) {
		if (worker_thread_get_memory_character(offset + i) == '\0') {
			return worker_thread_get_memory_ptr_void(offset, 1);
		}
	}
	return NULL;
}
