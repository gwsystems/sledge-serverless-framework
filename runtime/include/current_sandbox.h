#pragma once

#include <threads.h>

#include "current_wasm_module_instance.h"
#include "sandbox_types.h"
#include "listener_thread.h"

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
		sledge_abi__current_wasm_module_instance = (struct wasm_module_instance){
			/* Public */
			.abi =
			  (struct sledge_abi__wasm_module_instance){
			    .memory =
			      (struct sledge_abi__wasm_memory){
			        .size     = 0,
			        .capacity = 0,
			        .max      = 0,
			        .buffer   = NULL,
			      },
			    .table   = NULL,
			    .wasmg_0 = 0,
			  },
			/* Private */
			.wasi_context = NULL,
		};
		worker_thread_current_sandbox = NULL;
		/* This is because the event core does not maintain core-assigned deadline */
		if (!listener_thread_is_running()) runtime_worker_threads_deadline[worker_thread_idx] = UINT64_MAX;
	} else {
		sledge_abi__current_wasm_module_instance.wasi_context = sandbox->wasi_context;
		memcpy(&sledge_abi__current_wasm_module_instance.abi.memory, &sandbox->memory->abi,
		       sizeof(struct sledge_abi__wasm_memory));
		sledge_abi__current_wasm_module_instance.abi.table = sandbox->module->indirect_table;
		wasm_globals_update_if_used(&sandbox->globals, 0,
		                            &sledge_abi__current_wasm_module_instance.abi.wasmg_0);
		worker_thread_current_sandbox = sandbox;
		if (!listener_thread_is_running())
			runtime_worker_threads_deadline[worker_thread_idx] = sandbox->absolute_deadline;
	}
}

extern void current_sandbox_sleep();

static inline void *
current_sandbox_get_ptr_void(uint32_t offset, uint32_t bounds_check)
{
	assert(sledge_abi__current_wasm_module_instance.abi.memory.capacity > 0);
	return wasm_memory_get_ptr_void((struct wasm_memory *)&sledge_abi__current_wasm_module_instance.abi.memory,
	                                offset, bounds_check);
}

static inline char
current_sandbox_get_char(uint32_t offset)
{
	assert(sledge_abi__current_wasm_module_instance.abi.memory.capacity > 0);
	return wasm_memory_get_char((struct wasm_memory *)&sledge_abi__current_wasm_module_instance.abi.memory, offset);
}

static inline char *
current_sandbox_get_string(uint32_t offset, uint32_t size)
{
	return wasm_memory_get_string((struct wasm_memory *)&sledge_abi__current_wasm_module_instance.abi.memory,
	                              offset, size);
}

/**
 * Because we copy the members of a sandbox when it is set to current_sandbox, sledge_abi__current_wasm_module_instance
 * acts as a cache. If we change state by doing something like expanding a member, we have to perform writeback on the
 * sandbox member that we copied from.
 */
static inline void
current_sandbox_memory_writeback(void)
{
	struct sandbox *current_sandbox = current_sandbox_get();
	memcpy(&current_sandbox->memory->abi, &sledge_abi__current_wasm_module_instance.abi.memory,
	       sizeof(struct sledge_abi__wasm_memory));
}

static inline void
current_sandbox_trap(enum sledge_abi__wasm_trap trapno)
{
	assert(trapno != 0);
	assert(trapno < WASM_TRAP_COUNT);

	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_RUNNING_USER || sandbox->state == SANDBOX_RUNNING_SYS);

	siglongjmp(sandbox->ctxt.start_buf, trapno);
}

extern noreturn void current_sandbox_fini();
