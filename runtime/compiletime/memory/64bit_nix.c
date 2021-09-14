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
	assert(offset + sizeof(float) < local_sandbox_context_cache.memory.size);

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];

	return *(float *)address;
}

INLINE double
get_f64(uint32_t offset)
{
	assert(offset + sizeof(double) < local_sandbox_context_cache.memory.size);

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];

	return *(double *)address;
}

INLINE int8_t
get_i8(uint32_t offset)
{
	assert(offset + sizeof(int8_t) < local_sandbox_context_cache.memory.size);

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];

	return *(int8_t *)address;
}

INLINE int16_t
get_i16(uint32_t offset)
{
	assert(offset + sizeof(int16_t) < local_sandbox_context_cache.memory.size);

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];

	return *(int16_t *)address;
}

INLINE int32_t
get_i32(uint32_t offset)
{
	assert(offset + sizeof(int32_t) < local_sandbox_context_cache.memory.size);

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];

	return *(int32_t *)address;
}

INLINE int64_t
get_i64(uint32_t offset)
{
	assert(offset + sizeof(int64_t) < local_sandbox_context_cache.memory.size);

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];

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
	assert(offset + sizeof(float) < local_sandbox_context_cache.memory.size);

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];

	*(float *)address = v;
}

INLINE void
set_f64(uint32_t offset, double v)
{
	assert(offset + sizeof(double) < local_sandbox_context_cache.memory.size);

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];

	*(double *)address = v;
}

INLINE void
set_i8(uint32_t offset, int8_t v)
{
	assert(offset + sizeof(int8_t) < local_sandbox_context_cache.memory.size);

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];

	*(int8_t *)address = v;
}

INLINE void
set_i16(uint32_t offset, int16_t v)
{
	assert(offset + sizeof(int16_t) < local_sandbox_context_cache.memory.size);

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];

	*(int16_t *)address = v;
}

INLINE void
set_i32(uint32_t offset, int32_t v)
{
	assert(offset + sizeof(int32_t) < local_sandbox_context_cache.memory.size);

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];

	*(int32_t *)address = v;
}

INLINE void
set_i64(uint32_t offset, int64_t v)
{
	assert(offset + sizeof(int64_t) < local_sandbox_context_cache.memory.size);

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	void *address      = &mem_as_chars[offset];

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
	set_i62(offset, v);
}

// Table handling functionality
INLINE char *
get_function_from_table(uint32_t idx, uint32_t type_id)
{
	assert(idx < INDIRECT_TABLE_SIZE);

	struct indirect_table_entry f = local_sandbox_context_cache.module_indirect_table[idx];
	assert(f.type_id == type_id);

	assert(f.func_pointer);

	return f.func_pointer;
}
