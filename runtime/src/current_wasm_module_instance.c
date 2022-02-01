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
