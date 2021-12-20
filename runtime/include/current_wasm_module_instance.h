#pragma once

#include "wasm_module_instance.h"
#include "wasm_types.h"

extern thread_local struct wasm_module_instance current_wasm_module_instance;

extern void current_wasm_module_instance_memory_writeback(void);
extern void current_wasm_module_instance_trap(wasm_trap_t trapno);
