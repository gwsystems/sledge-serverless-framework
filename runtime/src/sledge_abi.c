#include "current_sandbox.h"
#include "sledge_abi.h"
#include "wasm_memory.h"

void
sledge_abi__wasm_trap_raise(enum sledge_abi__wasm_trap trapno)
{
	return current_sandbox_trap(trapno);
}

int32_t
sledge_abi__wasm_memory_expand(struct sledge_abi__wasm_memory *wasm_memory, uint32_t page_count)
{
	int32_t old_page_count = wasm_memory->size / WASM_PAGE_SIZE;

	int rc = wasm_memory_expand((struct wasm_memory *)wasm_memory, page_count * WASM_PAGE_SIZE);
	if (unlikely(rc == -1)) return -1;

	/* We updated "forked state" in sledge_abi__current_wasm_module_instance.memory. We need to write this back to
	 * the original struct as well  */
	current_sandbox_memory_writeback();

#ifdef LOG_SANDBOX_MEMORY_PROFILE
	// Cache the runtime of the first N page allocations
	for (int i = 0; i < page_count; i++) {
		if (likely(sandbox->timestamp_of.page_allocations_size < SANDBOX_PAGE_ALLOCATION_TIMESTAMP_COUNT)) {
			sandbox->timestamp_of.page_allocations[sandbox->timestamp_of.page_allocations_size++] =
			  sandbox->duration_of_state.running
			  + (uint32_t)(__getcycles() - sandbox->timestamp_of.last_state_change);
		}
	}
#endif

	return old_page_count;
}

void
sledge_abi__wasm_memory_initialize_region(struct sledge_abi__wasm_memory *wasm_memory, uint32_t offset,
                                          uint32_t region_size, uint8_t region[])
{
	return wasm_memory_initialize_region((struct wasm_memory *)wasm_memory, offset, region_size, region);
}

int32_t
sledge_abi__wasm_globals_get_i32(uint32_t idx)
{
	struct sandbox *current = current_sandbox_get();
	return wasm_globals_get_i32(current->globals, idx);
}

int64_t
sledge_abi__wasm_globals_get_i64(uint32_t idx)
{
	struct sandbox *current = current_sandbox_get();
	return wasm_globals_get_i64(current->globals, idx);
}

int32_t
sledge_abi__wasm_globals_set_i32(uint32_t idx, int32_t value)
{
	struct sandbox *current = current_sandbox_get();
	return wasm_globals_set_i32(current->globals, idx, value);
}

int32_t
sledge_abi__wasm_globals_set_i64(uint32_t idx, int64_t value)
{
	struct sandbox *current = current_sandbox_get();
	return wasm_globals_set_i64(current->globals, idx, value);
}
