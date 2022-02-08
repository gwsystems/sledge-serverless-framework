#pragma once
#include "wasm_memory.h"
#include "wasm_table.h"
#include "sledge_abi.h"

/* This structure is the runtime representation of the unique state of a module instance
 * Currently this is not spec-compliant, as it only supports a single table and a single memory and it excludes many
 * entities https://webassembly.github.io/spec/core/exec/runtime.html#module-instances
 */

struct wasm_module_instance {
	/* Public */
	struct sledge_abi__wasm_module_instance abi;
	/* Private */
	void *wasi_context;
};
