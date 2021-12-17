#pragma once

#include <threads.h>

#include "sandbox_types.h"
#include "current_wasm_module_instance.h"

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
		current_wasm_module_instance = (struct wasm_module_instance){
			.memory =
			  (struct wasm_memory){
			    .size     = 0,
			    .capacity = 0,
			    .max      = 0,
			    .buffer   = NULL,
			  },
			.table        = NULL,
			.wasi_context = NULL,
		};
		worker_thread_current_sandbox                      = NULL;
		runtime_worker_threads_deadline[worker_thread_idx] = UINT64_MAX;
	} else {
		memcpy(&current_wasm_module_instance.memory, sandbox->memory, sizeof(struct wasm_memory));
		current_wasm_module_instance.table                 = sandbox->module->indirect_table,
		worker_thread_current_sandbox                      = sandbox;
		runtime_worker_threads_deadline[worker_thread_idx] = sandbox->absolute_deadline;
		current_wasm_module_instance.wasi_context          = sandbox->wasi_context;
	}
}

extern void current_sandbox_sleep();

static inline void *
current_sandbox_get_ptr_void(uint32_t offset, uint32_t bounds_check)
{
	assert(current_wasm_module_instance.memory.capacity > 0);
	return wasm_memory_get_ptr_void(&current_wasm_module_instance.memory, offset, bounds_check);
}

static inline char
current_sandbox_get_char(uint32_t offset)
{
	assert(current_wasm_module_instance.memory.capacity > 0);
	return wasm_memory_get_char(&current_wasm_module_instance.memory, offset);
}

static inline char *
current_sandbox_get_string(uint32_t offset, uint32_t size)
{
	return wasm_memory_get_string(&current_wasm_module_instance.memory, offset, size);
}
