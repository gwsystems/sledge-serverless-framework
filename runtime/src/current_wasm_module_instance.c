#include <stdlib.h>

#include "current_sandbox.h"
#include "wasm_module_instance.h"
#include "wasm_memory.h"

thread_local struct wasm_module_instance current_wasm_module_instance = {
	.memory =
	  (struct wasm_memory){
	    .size     = 0,
	    .max      = 0,
	    .capacity = 0,
	    .buffer   = NULL,
	  },
	.table        = NULL,
	.wasi_context = NULL,
};

/**
 * Because we copy the members of a sandbox when it is set to current_sandbox, current_wasm_module_instance acts as a
 * cache. If we change state by doing something like expanding a member, we have to perform writeback on the sandbox
 * member that we copied from.
 */
void
current_wasm_module_instance_memory_writeback(void)
{
	struct sandbox *current_sandbox = current_sandbox_get();
	memcpy(current_sandbox->memory, &current_wasm_module_instance.memory, sizeof(struct wasm_memory));
}

void
current_wasm_module_instance_trap(wasm_trap_t trapno)
{
	assert(trapno != 0);
	assert(trapno < WASM_TRAP_COUNT);

	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_RUNNING_USER || sandbox->state == SANDBOX_RUNNING_SYS);

	longjmp(sandbox->ctxt.start_buf, trapno);
}
