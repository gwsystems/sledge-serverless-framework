#include <stdbool.h>
#include <stdint.h>

#include "sledge_abi.h"

int32_t
get_global_i32(uint32_t idx)
{
	return sledge_abi__wasm_globals_get_i32(idx);
}

int64_t
get_global_i64(uint32_t idx)
{
	return sledge_abi__wasm_globals_get_i64(idx);
}

void
set_global_i32(uint32_t idx, int32_t value)
{
	int rc = sledge_abi__wasm_globals_set_i32(idx, value);
	assert(rc == 0);
}

void
set_global_i64(uint32_t idx, int64_t value)
{
	int rc = sledge_abi__wasm_globals_set_i64(idx, value);
	assert(rc == 0);
}
