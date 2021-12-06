#pragma once
#include "wasm_memory.h"
#include "wasm_table.h"

/* This structure is the runtime representation of the unique state of a module instance
 * Currently this is not spec-compliant, as it only supports a single table and a single memory and it excludes many
 * entities https://webassembly.github.io/spec/core/exec/runtime.html#module-instances
 */
struct wasm_module_instance {
	struct wasm_memory *memory;
	struct wasm_table * table;
};

extern thread_local struct wasm_module_instance current_wasm_module_instance;
