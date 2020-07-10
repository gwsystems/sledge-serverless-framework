/* https://github.com/gwsystems/silverfish/blob/master/runtime/memory/64bit_nix.c */
#include "types.h"

#ifdef USE_MEM_VM

// All of these are pretty generic
INLINE float
get_f32(int32_t offset)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(float *)address;
}

INLINE double
get_f64(int32_t offset)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(double *)address;
}

INLINE i8
get_i8(int32_t offset)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(i8 *)address;
}

INLINE i16
get_i16(int32_t offset)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(i16 *)address;
}

INLINE int32_t
get_i32(int32_t offset)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(int32_t *)address;
}

INLINE int64_t
get_i64(int32_t offset)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(int64_t *)address;
}

INLINE int32_t
get_global_i32(int32_t offset)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(int32_t *)address;
}

INLINE int64_t
get_global_i64(int32_t offset)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(int64_t *)address;
}

// Now setting routines
INLINE void
set_f32(int32_t offset, float v)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(float *)address = v;
}

INLINE void
set_f64(int32_t offset, double v)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(double *)address = v;
}

INLINE void
set_i8(int32_t offset, i8 v)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(i8 *)address = v;
}

INLINE void
set_i16(int32_t offset, i16 v)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(i16 *)address = v;
}

INLINE void
set_i32(int32_t offset, int32_t v)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(int32_t *)address = v;
}

INLINE void
set_i64(int32_t offset, int64_t v)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(int64_t *)address = v;
}

INLINE void
set_global_i32(int32_t offset, int32_t v)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(int32_t *)address = v;
}

INLINE void
set_global_i64(int32_t offset, int64_t v)
{
	char *mem_as_chars = (char *)local_sandbox_context_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(int64_t *)address = v;
}

// Table handling functionality
INLINE char *
get_function_from_table(uint32_t idx, uint32_t type_id)
{
	assert(idx < INDIRECT_TABLE_SIZE);

	struct indirect_table_entry f = local_sandbox_context_cache.module_indirect_table[idx];

	//	assert(f.type_id == type_id);
	assert(f.func_pointer);

	return f.func_pointer;
}
#else
#error "Incorrect memory module!"
#endif
