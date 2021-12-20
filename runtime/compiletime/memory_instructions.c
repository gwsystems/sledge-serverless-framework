#include <assert.h>

#include "current_wasm_module_instance.h"

INLINE uint32_t
instruction_memory_size()
{
	return (uint32_t)(current_wasm_module_instance.memory.size / WASM_PAGE_SIZE);
}

// These functions are equivalent to those in wasm_memory.h, but they minimize pointer dereferencing
INLINE float
get_f32(uint32_t offset)
{
	assert(current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(float) > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "get_f32 OOB: offset %u + length %u > size %lu\n", offset, (uint32_t)sizeof(float),
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	return *(float *)&current_wasm_module_instance.memory.buffer[offset];
}

INLINE double
get_f64(uint32_t offset)
{
	assert(current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(double) > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "get_f64 OOB: offset %u + length %u > size %lu\n", offset, (uint32_t)sizeof(double),
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	return *(double *)&current_wasm_module_instance.memory.buffer[offset];
}

INLINE int8_t
get_i8(uint32_t offset)
{
	assert(current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int8_t) > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "get_i8 OOB: offset %u + length %u > size %lu\n", offset, (uint32_t)sizeof(int8_t),
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	return *(int8_t *)&current_wasm_module_instance.memory.buffer[offset];
}

INLINE int16_t
get_i16(uint32_t offset)
{
	assert(current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int16_t) > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "get_i16 OOB: offset %u + length %u > size %lu\n", offset, (uint32_t)sizeof(int16_t),
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	return *(int16_t *)&current_wasm_module_instance.memory.buffer[offset];
}

INLINE int32_t
get_i32(uint32_t offset)
{
	assert(current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int32_t) > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "get_i32 OOB: offset %u + length %u > size %lu\n", offset, (uint32_t)sizeof(int32_t),
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	return *(int32_t *)&current_wasm_module_instance.memory.buffer[offset];
}

INLINE int64_t
get_i64(uint32_t offset)
{
	assert(current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int64_t) > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "get_i64 OOB: offset %u + length %u > size %lu\n", offset, (uint32_t)sizeof(int64_t),
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	return *(int64_t *)&current_wasm_module_instance.memory.buffer[offset];
}

INLINE int32_t
get_global_i32(uint32_t offset)
{
	return get_i32(offset);
}

INLINE int64_t
get_global_i64(uint32_t offset)
{
	return get_i64(offset);
}

// Now setting routines
INLINE void
set_f32(uint32_t offset, float value)
{
	assert(current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(float) > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "set_f32 OOB: offset %u + length %u > size %lu\n", offset, (uint32_t)sizeof(float),
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	*(float *)&current_wasm_module_instance.memory.buffer[offset] = value;
}

INLINE void
set_f64(uint32_t offset, double value)
{
	assert(current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(double) > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "set_f64 OOB: offset %u + length %u > size %lu\n", offset, (uint32_t)sizeof(double),
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	*(double *)&current_wasm_module_instance.memory.buffer[offset] = value;
}

INLINE void
set_i8(uint32_t offset, int8_t value)
{
	assert(current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int8_t) > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "set_i8 OOB: offset %u + length %u > size %lu\n", offset, (uint32_t)sizeof(int8_t),
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	*(int8_t *)&current_wasm_module_instance.memory.buffer[offset] = value;
}

INLINE void
set_i16(uint32_t offset, int16_t value)
{
	assert(current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int16_t) > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "set_i16 OOB: offset %u + length %u > size %lu\n", offset, (uint32_t)sizeof(int16_t),
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	*(int16_t *)&current_wasm_module_instance.memory.buffer[offset] = value;
}

INLINE void
set_i32(uint32_t offset, int32_t value)
{
	assert(current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int32_t) > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "set_i32 OOB: offset %u + length %u > size %lu\n", offset, (uint32_t)sizeof(int32_t),
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	*(int32_t *)&current_wasm_module_instance.memory.buffer[offset] = value;
}

INLINE void
set_i64(uint32_t offset, int64_t value)
{
	assert(current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int64_t) > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "set_i64 OOB: offset %u + length %u > size %lu\n", offset, (uint32_t)sizeof(int64_t),
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	*(int64_t *)&current_wasm_module_instance.memory.buffer[offset] = value;
}

INLINE void
set_global_i32(uint32_t offset, int32_t value)
{
	set_i32(offset, value);
}

INLINE void
set_global_i64(uint32_t offset, int64_t value)
{
	set_i64(offset, value);
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
	int old_page_count = current_wasm_module_instance.memory.size / WASM_PAGE_SIZE;

	/* Return -1 if we've hit the linear memory max */
	int rc = wasm_memory_expand(&current_wasm_module_instance.memory, WASM_PAGE_SIZE * count);
	if (unlikely(rc == -1)) return -1;

	/* We updated "forked state" in current_wasm_module_instance.memory. We need to write this back to persist  */
	current_wasm_module_instance_memory_writeback();

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


INLINE void
initialize_region(uint32_t offset, uint32_t region_size, uint8_t region[region_size])
{
	wasm_memory_initialize_region(&current_wasm_module_instance.memory, offset, region_size, region);
}
