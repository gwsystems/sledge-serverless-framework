#include <assert.h>
#include <string.h>

#include "runtime.h"
#include "types.h"

/* Region initialization helper function */
EXPORT void
initialize_region(uint32_t offset, uint32_t region_size, uint8_t region[region_size])
{
	assert(local_sandbox_context_cache.memory->size >= region_size);
	assert(offset < local_sandbox_context_cache.memory->size - region_size);

	memcpy(get_memory_ptr_for_runtime(offset, region_size), region, region_size);
}

void
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

/* If we are using runtime globals, we need to populate them */
WEAK void
populate_globals()
{
	assert(0); /* FIXME: is this used in WASM as dynamic modules? Issue #105. */
}
