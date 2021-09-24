#include <assert.h>
#include "types.h"

extern void current_sandbox_trap(wasm_trap_t trapno);

uint32_t
instruction_memory_size()
{
	return local_sandbox_context_cache.memory.size / WASM_PAGE_SIZE;
}

// All of these are pretty generic
INLINE float
get_f32(uint32_t offset)
{
	if (unlikely(offset + (uint32_t)sizeof(float) > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, (uint32_t)sizeof(float),
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];
#ifdef LOG_LINEAR_MEMORY
	fprintf(stderr, "get_f32 offset: %u, value: %f\n", offset, *(float *)address);
#endif

	return *(float *)address;
}

INLINE double
get_f64(uint32_t offset)
{
	if (unlikely(offset + (uint32_t)sizeof(double) > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, (uint32_t)sizeof(double),
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];
#ifdef LOG_LINEAR_MEMORY
	fprintf(stderr, "get_f64 offset: %u, value: %lf\n", offset, *(double *)address);
#endif

	return *(double *)address;
}

INLINE int8_t
get_i8(uint32_t offset)
{
	if (unlikely(offset + (uint32_t)sizeof(int8_t) > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, (uint32_t)sizeof(int8_t),
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];
#ifdef LOG_LINEAR_MEMORY
	fprintf(stderr, "get_i8 offset: %u, value: %d\n", offset, *(int8_t *)address);
#endif

	return *(int8_t *)address;
}

INLINE int16_t
get_i16(uint32_t offset)
{
	if (unlikely(offset + (uint32_t)sizeof(int16_t) > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, (uint32_t)sizeof(int16_t),
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];
#ifdef LOG_LINEAR_MEMORY
	fprintf(stderr, "get_i16 offset: %u, value: %d\n", offset, *(int16_t *)address);
#endif

	return *(int16_t *)address;
}

INLINE int32_t
get_i32(uint32_t offset)
{
	if (unlikely(offset + (uint32_t)sizeof(int32_t) > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, (uint32_t)sizeof(int32_t),
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];
#ifdef LOG_LINEAR_MEMORY
	fprintf(stderr, "get_i32 offset: %u, value: %d\n", offset, *(int32_t *)address);
#endif

	return *(int32_t *)address;
}

INLINE int64_t
get_i64(uint32_t offset)
{
	if (unlikely(offset + (uint32_t)sizeof(int64_t) > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, (uint32_t)sizeof(int64_t),
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];
#ifdef LOG_LINEAR_MEMORY
	fprintf(stderr, "get_i64 offset: %u, value: %ld\n", offset, *(int64_t *)address);
#endif

	return *(int64_t *)address;
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
set_f32(uint32_t offset, float v)
{
	if (unlikely(offset + (uint32_t)sizeof(float) > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, (uint32_t)sizeof(float),
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];
#ifdef LOG_LINEAR_MEMORY
	fprintf(stderr, "set_f32 offset: %u, value: %f\n", offset, v);
#endif

	*(float *)address = v;
}

INLINE void
set_f64(uint32_t offset, double v)
{
	if (unlikely(offset + (uint32_t)sizeof(double) > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, (uint32_t)sizeof(double),
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];
#ifdef LOG_LINEAR_MEMORY
	fprintf(stderr, "set_f64 offset: %u, value: %lf\n", offset, v);
#endif

	*(double *)address = v;
}

INLINE void
set_i8(uint32_t offset, int8_t v)
{
	if (unlikely(offset + (uint32_t)sizeof(int8_t) > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, (uint32_t)sizeof(int8_t),
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];
#ifdef LOG_LINEAR_MEMORY
	fprintf(stderr, "set_i8 offset: %u, value: %d\n", offset, v);
#endif

	*(int8_t *)address = v;
}

INLINE void
set_i16(uint32_t offset, int16_t v)
{
	if (unlikely(offset + (uint32_t)sizeof(int16_t) > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, (uint32_t)sizeof(int16_t),
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];
#ifdef LOG_LINEAR_MEMORY
	fprintf(stderr, "set_i16 offset: %u, value: %d\n", offset, v);
#endif

	*(int16_t *)address = v;
}

INLINE void
set_i32(uint32_t offset, int32_t v)
{
	if (unlikely(offset + (uint32_t)sizeof(int32_t) > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, (uint32_t)sizeof(int32_t),
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];
#ifdef LOG_LINEAR_MEMORY
	fprintf(stderr, "set_i32 offset: %u, value: %d\n", offset, v);
#endif

	*(int32_t *)address = v;
}

INLINE void
set_i64(uint32_t offset, int64_t v)
{
	if (unlikely(offset + (uint32_t)sizeof(int64_t) > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, (uint32_t)sizeof(int64_t),
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];
#ifdef LOG_LINEAR_MEMORY
	fprintf(stderr, "set_i64 offset: %u, value: %ld\n", offset, v);
#endif

	*(int64_t *)address = v;
}

INLINE void
set_global_i32(uint32_t offset, int32_t v)
{
	set_i32(offset, v);
}

INLINE void
set_global_i64(uint32_t offset, int64_t v)
{
	set_i64(offset, v);
}

// Table handling functionality
// char *
// get_function_from_table(uint32_t idx, uint32_t type_id)
// {
// 	if (unlikely(idx >= INDIRECT_TABLE_SIZE)) {
// 		fprintf(stderr, "idx: %u, Table size: %u\n", idx, INDIRECT_TABLE_SIZE);
// 		current_sandbox_trap(WASM_TRAP_INVALID_INDEX);
// 	}

// 	struct indirect_table_entry f = local_sandbox_context_cache.module_indirect_table[idx];
// 	if (unlikely(f.type_id != type_id)) {
// 		fprintf(stderr, "Function Type mismatch. Expected: %u, Actual: %u\n", type_id, f.type_id);
// 		current_sandbox_trap(WASM_TRAP_MISMATCHED_FUNCTION_TYPE);
// 	}
// 	if (unlikely(f.func_pointer == NULL)) {
// 		fprintf(stderr, "Function Type mismatch. Index %u resolved to NULL Pointer\n", idx);
// 		current_sandbox_trap(WASM_TRAP_MISMATCHED_FUNCTION_TYPE);
// 	}

// 	return f.func_pointer;
// }
