#pragma once
#include "wasm_memory.h"
#include "wasm_table.h"
#include "sledge_abi.h"

struct wasm_module_instance {
	/* Public */
	struct sledge_abi__wasm_module_instance abi;
	/* Private */
	void *wasi_context;
};
