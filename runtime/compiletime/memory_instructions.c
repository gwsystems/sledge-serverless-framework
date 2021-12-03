#include <assert.h>
#include <assert.h>
#include <math.h>

#include "wasm_store.h"

extern thread_local struct wasm_module_instance current_wasm_module_instance;

INLINE void
initialize_region(uint32_t offset, uint32_t region_size, uint8_t region[region_size])
{
	wasm_memory_initialize_region(current_wasm_module_instance.memory, offset, region_size, region);
}

INLINE uint32_t
instruction_memory_size()
{
	return wasm_memory_get_page_count(current_wasm_module_instance.memory);
}

/**
 * @brief Stub that implements the WebAssembly memory.grow instruction
 *
 * @param count number of pages to grow the WebAssembly linear memory by
 * @return The previous size of the linear memory in pages or -1 if enough memory cannot be allocated
 */
INLINE int32_t
instruction_memory_grow(uint32_t count)
{
	int old_page_count = current_wasm_module_instance.memory->size / WASM_PAGE_SIZE;

	/* Return -1 if we've hit the linear memory max */
	int rc = wasm_memory_expand(current_wasm_module_instance.memory, WASM_PAGE_SIZE * count);
	if (unlikely(rc == -1)) return -1;

#ifdef LOG_SANDBOX_MEMORY_PROFILE
	// Cache the runtime of the first N page allocations
	for (int i = 0; i < count; i++) {
		if (likely(sandbox->timestamp_of.page_allocations_size < SANDBOX_PAGE_ALLOCATION_TIMESTAMP_COUNT)) {
			sandbox->timestamp_of.page_allocations[sandbox->timestamp_of.page_allocations_size++] =
			  sandbox->duration_of_state.running
			  + (uint32_t)(__getcycles() - sandbox->timestamp_of.last_state_change);
		}
	}
#endif

	return rc;
}

INLINE float
get_f32(uint32_t offset)
{
	return wasm_memory_get_float(current_wasm_module_instance.memory, offset);
}

INLINE void
set_f32(uint32_t offset, float v)
{
	wasm_memory_set_float(current_wasm_module_instance.memory, offset, v);
}

INLINE double
get_f64(uint32_t offset)
{
	return wasm_memory_get_double(current_wasm_module_instance.memory, offset);
}

INLINE void
set_f64(uint32_t offset, double v)
{
	wasm_memory_set_double(current_wasm_module_instance.memory, offset, v);
}

INLINE int8_t
get_i8(uint32_t offset)
{
	return wasm_memory_get_int8(current_wasm_module_instance.memory, offset);
}

INLINE void
set_i8(uint32_t offset, int8_t v)
{
	wasm_memory_set_int8(current_wasm_module_instance.memory, offset, v);
}

INLINE int16_t
get_i16(uint32_t offset)
{
	return wasm_memory_get_int16(current_wasm_module_instance.memory, offset);
}

INLINE void
set_i16(uint32_t offset, int16_t v)
{
	wasm_memory_set_int16(current_wasm_module_instance.memory, offset, v);
}

INLINE int32_t
get_i32(uint32_t offset)
{
	return wasm_memory_get_int32(current_wasm_module_instance.memory, offset);
}

INLINE void
set_i32(uint32_t offset, int32_t v)
{
	wasm_memory_set_int32(current_wasm_module_instance.memory, offset, v);
}

INLINE int64_t
get_i64(uint32_t offset)
{
	return wasm_memory_get_int64(current_wasm_module_instance.memory, offset);
}

INLINE void
set_i64(uint32_t offset, int64_t v)
{
	wasm_memory_set_int64(current_wasm_module_instance.memory, offset, v);
}

INLINE int32_t
get_global_i32(uint32_t offset)
{
	return get_i32(offset);
}

INLINE void
set_global_i32(uint32_t offset, int32_t v)
{
	set_i32(offset, v);
}

INLINE int64_t
get_global_i64(uint32_t offset)
{
	return get_i64(offset);
}

INLINE void
set_global_i64(uint32_t offset, int64_t v)
{
	set_i64(offset, v);
}
