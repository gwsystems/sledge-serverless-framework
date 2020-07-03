/* https://github.com/gwsystems/silverfish/blob/master/runtime/memory/64bit_nix.c */
#include "types.h"

#ifdef USE_MEM_VM

// All of these are pretty generic
INLINE float
get_f32(i32 offset)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(float *)address;
}

INLINE double
get_f64(i32 offset)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(double *)address;
}

INLINE i8
get_i8(i32 offset)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(i8 *)address;
}

INLINE i16
get_i16(i32 offset)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(i16 *)address;
}

INLINE i32
get_i32(i32 offset)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(i32 *)address;
}

INLINE i64
get_i64(i32 offset)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(i64 *)address;
}

INLINE i32
get_global_i32(i32 offset)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(i32 *)address;
}

INLINE i64
get_global_i64(i32 offset)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	return *(i64 *)address;
}

// Now setting routines
INLINE void
set_f32(i32 offset, float v)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(float *)address = v;
}

INLINE void
set_f64(i32 offset, double v)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(double *)address = v;
}

INLINE void
set_i8(i32 offset, i8 v)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(i8 *)address = v;
}

INLINE void
set_i16(i32 offset, i16 v)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(i16 *)address = v;
}

INLINE void
set_i32(i32 offset, i32 v)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(i32 *)address = v;
}

INLINE void
set_i64(i32 offset, i64 v)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(i64 *)address = v;
}

INLINE void
set_global_i32(i32 offset, i32 v)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(i32 *)address = v;
}

INLINE void
set_global_i64(i32 offset, i64 v)
{
	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	void *address      = &mem_as_chars[offset];

	*(i64 *)address = v;
}

// Table handling functionality
INLINE char *
get_function_from_table(u32 idx, u32 type_id)
{
	assert(idx < INDIRECT_TABLE_SIZE);

	struct indirect_table_entry f = local_sandbox_member_cache.module_indirect_table[idx];

	//	assert(f.type_id == type_id);
	assert(f.func_pointer);

	return f.func_pointer;
}
#else
#error "Incorrect memory module!"
#endif
