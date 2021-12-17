#pragma once

#include "wasm_module_instance.h"

extern thread_local struct wasm_module_instance current_wasm_module_instance;

extern void current_wasm_module_instance_memory_writeback(void);
