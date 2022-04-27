#include <stdbool.h>
#include <stdint.h>

#include "sledge_abi.h"

/* This is private and NOT in the sledge_abi.h header because the runtime uses an overlay struct that extends this
 * symbol with private members */
extern thread_local struct sledge_abi__wasm_module_instance sledge_abi__current_wasm_module_instance;

int32_t
get_global_i32(uint32_t idx)
{
	if (idx == 0) {
		return (int32_t)sledge_abi__current_wasm_module_instance.wasmg_0;
	} else {
		return sledge_abi__wasm_globals_get_i32(idx);
	}
}

int64_t
get_global_i64(uint32_t idx)
{
	if (idx == 0) {
		return (int64_t)sledge_abi__current_wasm_module_instance.wasmg_0;
	} else {
		return sledge_abi__wasm_globals_get_i64(idx);
	}
}

void
set_global_i32(uint32_t idx, int32_t value)
{
	if (idx == 0) {
		sledge_abi__current_wasm_module_instance.wasmg_0 = (uint64_t)value;
	} else {
		/* aWsm does not currently pass the is_mutable flag, so all runtime globals are assumed to be mutable.
		This is true if aWsm uses the flags to inline constant globals */
		sledge_abi__wasm_globals_set_i32(idx, value, true);
	}
}

void
set_global_i64(uint32_t idx, int64_t value)
{
	if (idx == 0) {
		sledge_abi__current_wasm_module_instance.wasmg_0 = (uint64_t)value;
	} else {
		/* aWsm does not currently pass the is_mutable flag, so all runtime globals are assumed to be mutable.
		This is true if aWsm uses the flags to inline constant globals */
		sledge_abi__wasm_globals_set_i64(idx, value, true);
	}
}
