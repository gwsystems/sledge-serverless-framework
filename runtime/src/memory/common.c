#include <runtime.h>

__thread struct indirect_table_entry *module_indirect_table = NULL;
__thread void *                       sandbox_lmbase        = NULL;
__thread u32                          sandbox_lmbound       = 0;

// Region initialization helper function
EXPORT void
initialize_region(u32 offset, u32 data_count, char *data)
{
	assert(sandbox_lmbound >= data_count);
	assert(offset < sandbox_lmbound - data_count);

	// FIXME: Hack around segmented and unsegmented access
	memcpy(get_memory_ptr_for_runtime(offset, data_count), data, data_count);
}

void
add_function_to_table(u32 idx, u32 type_id, char *pointer)
{
	assert(idx < INDIRECT_TABLE_SIZE);

	// TODO: atomic for multiple concurrent invocations?
	if (module_indirect_table[idx].type_id == type_id && module_indirect_table[idx].func_pointer == pointer) return;

	module_indirect_table[idx] = (struct indirect_table_entry){ .type_id = type_id, .func_pointer = pointer };
}

// If we are using runtime globals, we need to populate them
WEAK void
populate_globals()
{
	assert(0); // FIXME: is this used in WASM as dynamic modules?
}
