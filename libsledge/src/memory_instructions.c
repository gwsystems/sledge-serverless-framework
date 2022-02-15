#include "sledge_abi.h"

#define INLINE         __attribute__((always_inline))
#define WASM_PAGE_SIZE (1024 * 64) /* 64KB */

/* This is private and NOT in the sledge_abi.h header because the runtime uses an overlay struct that extends this
 * symbol with private members */
extern thread_local struct sledge_abi__wasm_module_instance sledge_abi__current_wasm_module_instance;

INLINE uint32_t
instruction_memory_size()
{
	return (uint32_t)(sledge_abi__current_wasm_module_instance.memory.size / (uint64_t)WASM_PAGE_SIZE);
}

// These functions are equivalent to those in wasm_memory.h, but they minimize pointer dereferencing
INLINE float
get_f32(uint32_t offset)
{
	assert(sledge_abi__current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(float)
	             > sledge_abi__current_wasm_module_instance.memory.size)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	return *(float *)&sledge_abi__current_wasm_module_instance.memory.buffer[offset];
}

INLINE double
get_f64(uint32_t offset)
{
	assert(sledge_abi__current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(double)
	             > sledge_abi__current_wasm_module_instance.memory.size)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	return *(double *)&sledge_abi__current_wasm_module_instance.memory.buffer[offset];
}

INLINE int8_t
get_i8(uint32_t offset)
{
	assert(sledge_abi__current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int8_t)
	             > sledge_abi__current_wasm_module_instance.memory.size)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	return *(int8_t *)&sledge_abi__current_wasm_module_instance.memory.buffer[offset];
}

INLINE int16_t
get_i16(uint32_t offset)
{
	assert(sledge_abi__current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int16_t)
	             > sledge_abi__current_wasm_module_instance.memory.size)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	return *(int16_t *)&sledge_abi__current_wasm_module_instance.memory.buffer[offset];
}

INLINE int32_t
get_i32(uint32_t offset)
{
	assert(sledge_abi__current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int32_t)
	             > sledge_abi__current_wasm_module_instance.memory.size)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	return *(int32_t *)&sledge_abi__current_wasm_module_instance.memory.buffer[offset];
}

INLINE int64_t
get_i64(uint32_t offset)
{
	assert(sledge_abi__current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int64_t)
	             > sledge_abi__current_wasm_module_instance.memory.size)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	return *(int64_t *)&sledge_abi__current_wasm_module_instance.memory.buffer[offset];
}

// Now setting routines
INLINE void
set_f32(uint32_t offset, float value)
{
	assert(sledge_abi__current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(float)
	             > sledge_abi__current_wasm_module_instance.memory.size)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	*(float *)&sledge_abi__current_wasm_module_instance.memory.buffer[offset] = value;
}

INLINE void
set_f64(uint32_t offset, double value)
{
	assert(sledge_abi__current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(double)
	             > sledge_abi__current_wasm_module_instance.memory.size)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	*(double *)&sledge_abi__current_wasm_module_instance.memory.buffer[offset] = value;
}

INLINE void
set_i8(uint32_t offset, int8_t value)
{
	assert(sledge_abi__current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int8_t)
	             > sledge_abi__current_wasm_module_instance.memory.size)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	*(int8_t *)&sledge_abi__current_wasm_module_instance.memory.buffer[offset] = value;
}

INLINE void
set_i16(uint32_t offset, int16_t value)
{
	assert(sledge_abi__current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int16_t)
	             > sledge_abi__current_wasm_module_instance.memory.size)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	*(int16_t *)&sledge_abi__current_wasm_module_instance.memory.buffer[offset] = value;
}

INLINE void
set_i32(uint32_t offset, int32_t value)
{
	assert(sledge_abi__current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int32_t)
	             > sledge_abi__current_wasm_module_instance.memory.size)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	*(int32_t *)&sledge_abi__current_wasm_module_instance.memory.buffer[offset] = value;
}

INLINE void
set_i64(uint32_t offset, int64_t value)
{
	assert(sledge_abi__current_wasm_module_instance.memory.buffer != NULL);

	if (unlikely((uint64_t)offset + (uint64_t)sizeof(int64_t)
	             > sledge_abi__current_wasm_module_instance.memory.size)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	*(int64_t *)&sledge_abi__current_wasm_module_instance.memory.buffer[offset] = value;
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
	return sledge_abi__wasm_memory_expand(&sledge_abi__current_wasm_module_instance.memory, count);
}


INLINE void
initialize_region(uint32_t offset, uint32_t region_size, uint8_t region[region_size])
{
	sledge_abi__wasm_memory_initialize_region(&sledge_abi__current_wasm_module_instance.memory, offset, region_size,
	                                          region);
}
