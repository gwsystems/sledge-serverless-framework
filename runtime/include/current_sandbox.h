#pragma once

#include <threads.h>

#include "sandbox_types.h"
#include "sandbox_context_cache.h"

/* current sandbox that is active.. */
extern thread_local struct sandbox *worker_thread_current_sandbox;

void current_sandbox_start(void);

/**
 * Getter for the current sandbox executing on this thread
 * @returns the current sandbox executing on this thread
 */
static inline struct sandbox *
current_sandbox_get(void)
{
	return worker_thread_current_sandbox;
}

/**
 * Setter for the current sandbox executing on this thread
 * @param sandbox the sandbox we are setting this thread to run
 */
static inline void
current_sandbox_set(struct sandbox *sandbox)
{
	/* Unpack hierarchy to avoid pointer chasing */
	if (sandbox == NULL) {
		local_sandbox_context_cache = (struct sandbox_context_cache){
			.memory                = NULL,
			.module_indirect_table = NULL,
		};
		worker_thread_current_sandbox                      = NULL;
		runtime_worker_threads_deadline[worker_thread_idx] = UINT64_MAX;
	} else {
		local_sandbox_context_cache = (struct sandbox_context_cache){
			.memory                = sandbox->memory,
			.module_indirect_table = sandbox->module->indirect_table,
		};
		worker_thread_current_sandbox                      = sandbox;
		runtime_worker_threads_deadline[worker_thread_idx] = sandbox->absolute_deadline;
	}
}

extern void current_sandbox_sleep();

static inline void *
current_sandbox_get_ptr_void(uint32_t offset, uint32_t bounds_check)
{
	assert(local_sandbox_context_cache.memory != NULL);
	return wasm_linear_memory_get_ptr_void(local_sandbox_context_cache.memory, offset, bounds_check);
}

static inline char
current_sandbox_get_char(uint32_t offset)
{
	assert(local_sandbox_context_cache.memory != NULL);
	return wasm_linear_memory_get_char(local_sandbox_context_cache.memory, offset);
}

static inline char *
current_sandbox_get_string(uint32_t offset, uint32_t size)
{
	return wasm_linear_memory_get_string(local_sandbox_context_cache.memory, offset, size);
}
