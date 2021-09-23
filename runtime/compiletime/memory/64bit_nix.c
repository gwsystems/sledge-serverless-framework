#include <assert.h>
#include "types.h"

uint32_t
instruction_memory_size()
{
	return local_sandbox_context_cache.memory.size / WASM_PAGE_SIZE;
}

// All of these are pretty generic
INLINE float
get_f32(uint32_t offset)
{
	assert(offset + sizeof(float) <= local_sandbox_context_cache.memory.size);

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
	assert(offset + sizeof(double) <= local_sandbox_context_cache.memory.size);

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
	assert(offset + sizeof(int8_t) <= local_sandbox_context_cache.memory.size);

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
	assert(offset + sizeof(int16_t) <= local_sandbox_context_cache.memory.size);

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
	assert(offset + sizeof(int32_t) <= local_sandbox_context_cache.memory.size);

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
	assert(offset + sizeof(int64_t) <= local_sandbox_context_cache.memory.size);

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
	assert(offset + sizeof(float) <= local_sandbox_context_cache.memory.size);

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
	assert(offset + sizeof(double) <= local_sandbox_context_cache.memory.size);

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
	assert(offset + sizeof(int8_t) <= local_sandbox_context_cache.memory.size);

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
	assert(offset + sizeof(int16_t) <= local_sandbox_context_cache.memory.size);

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
	assert(offset + sizeof(int32_t) <= local_sandbox_context_cache.memory.size);

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
	assert(offset + sizeof(int64_t) <= local_sandbox_context_cache.memory.size);

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
// INLINE char *
// get_function_from_table(uint32_t idx, uint32_t type_id)
// {
// #ifdef LOG_FUNCTION_TABLE
// 	fprintf(stderr, "get_function_from_table(idx: %u, type_id: %u)\n", idx, type_id);
// 	fprintf(stderr, "indirect_table_size: %u\n", INDIRECT_TABLE_SIZE);
// #endif
// 	assert(idx < INDIRECT_TABLE_SIZE);

// 	struct indirect_table_entry f = local_sandbox_context_cache.module_indirect_table[idx];
// #ifdef LOG_FUNCTION_TABLE
// 	fprintf(stderr, "assumed type: %u, type in table: %u\n", type_id, f.type_id);
// #endif
// 	// FIXME: Commented out function type check because of gocr
// 	// assert(f.type_id == type_id);

// 	assert(f.func_pointer != NULL);

// 	return f.func_pointer;
// }
