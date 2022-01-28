#include <stdlib.h>

#include "current_sandbox.h"
#include "wasm_module_instance.h"
#include "wasm_memory.h"

thread_local struct wasm_module_instance sledge_abi__current_wasm_module_instance = {
	.abi.memory =
	  (struct sledge_abi__wasm_memory){
	    .size     = 0,
	    .max      = 0,
	    .capacity = 0,
	    .buffer   = NULL,
	  },
	.abi.table    = NULL,
	.abi.wasmg_0  = 0xDEADBEEF,
	.wasi_context = NULL,
};

void
sledge_abi__current_wasm_module_instance_trap(enum sledge_abi__wasm_trap trapno)
{
	assert(trapno != 0);
	assert(trapno < WASM_TRAP_COUNT);

	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_RUNNING_USER || sandbox->state == SANDBOX_RUNNING_SYS);

	longjmp(sandbox->ctxt.start_buf, trapno);
}
