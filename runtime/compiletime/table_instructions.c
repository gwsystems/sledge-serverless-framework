
#include "types.h"

extern thread_local struct wasm_module_instance current_wasm_module_instance;

INLINE void
add_function_to_table(uint32_t idx, uint32_t type_id, char *pointer)
{
	assert(idx < INDIRECT_TABLE_SIZE);
	assert(local_sandbox_context_cache.module_indirect_table != NULL);

	/* TODO: atomic for multiple concurrent invocations? Issue #97 */
	if (local_sandbox_context_cache.module_indirect_table[idx].type_id == type_id
	    && local_sandbox_context_cache.module_indirect_table[idx].func_pointer == pointer)
		return;

	local_sandbox_context_cache.module_indirect_table[idx] = (struct indirect_table_entry){
		.type_id = type_id, .func_pointer = pointer
	};
}

INLINE char *
get_function_from_table(uint32_t idx, uint32_t type_id)
{
#ifdef LOG_FUNCTION_TABLE
	fprintf(stderr, "get_function_from_table(idx: %u, type_id: %u)\n", idx, type_id);
	fprintf(stderr, "indirect_table_size: %u\n", INDIRECT_TABLE_SIZE);
#endif
	assert(idx < INDIRECT_TABLE_SIZE);

	struct indirect_table_entry f = local_sandbox_context_cache.module_indirect_table[idx];
#ifdef LOG_FUNCTION_TABLE
	fprintf(stderr, "assumed type: %u, type in table: %u\n", type_id, f.type_id);
#endif
	// FIXME: Commented out function type check because of gocr
	// assert(f.type_id == type_id);

	assert(f.func_pointer != NULL);

	return f.func_pointer;
}
