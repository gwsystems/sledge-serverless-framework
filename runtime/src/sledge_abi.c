#include "current_sandbox.h"
#include "sledge_abi.h"
#include "wasm_memory.h"


/**
 * Because we copy the members of a sandbox when it is set to current_sandbox, sledge_abi__current_wasm_module_instance
 * acts as a cache. If we change state by doing something like expanding a member, we have to perform writeback on the
 * sandbox member that we copied from.
 */
void
sledge_abi__wasm_memory_writeback(void)
{
	return current_sandbox_memory_writeback();
}

void
sledge_abi__wasm_trap_raise(enum sledge_abi__wasm_trap trapno)
{
	return current_sandbox_trap(trapno);
}

int
sledge_abi__wasm_memory_expand(struct sledge_abi__wasm_memory *wasm_memory, size_t size_to_expand)
{
	return wasm_memory_expand((struct wasm_memory *)wasm_memory, size_to_expand);
}

void
sledge_abi__wasm_memory_initialize_region(struct sledge_abi__wasm_memory *wasm_memory, uint32_t offset,
                                          uint32_t region_size, uint8_t region[])
{
	return wasm_memory_initialize_region((struct wasm_memory *)wasm_memory, offset, region_size, region);
}
