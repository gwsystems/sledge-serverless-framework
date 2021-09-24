#include <assert.h>
#include <string.h>

#include "runtime.h"
#include "types.h"

/* Region initialization helper function */
/**
 * @brief Initialize a region, copying it into linear memory
 *
 * This is a stub called by *.so modules
 *
 * @param dest_offset target offset in linear memory
 * @param n the size of the region
 * @param src address of region to copy
 */
EXPORT void
initialize_region(uint32_t dest_offset, uint32_t n, char src[n])
{
	assert(local_sandbox_context_cache.memory.size >= n);
	assert(dest_offset < local_sandbox_context_cache.memory.size - n);

	memcpy(get_memory_ptr(dest_offset, n), src, n);
}

void
add_function_to_table(uint32_t idx, uint32_t type_id, char *pointer)
{
	/* Can't trap because this is statically performed as part of initialization */
	assert(idx < INDIRECT_TABLE_SIZE);
	assert(local_sandbox_context_cache.module_indirect_table != NULL);
	assert(pointer != NULL);

	/* We may be registering the same module on multiple ports */
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
