#pragma once
#include "wasm_linear_memory.h"
#include "wasm_indirect_table.h"

/* Cache of Frequently Accessed Members used to avoid pointer chasing */
struct sandbox_context_cache {
	struct wasm_linear_memory *  memory;
	struct indirect_table_entry *module_indirect_table;
};

extern thread_local struct sandbox_context_cache local_sandbox_context_cache;
