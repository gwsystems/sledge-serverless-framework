#include <assert.h>
#include <string.h>

#include "runtime.h"
#include "types.h"

/* Region initialization helper function */
EXPORT void
initialize_region(uint32_t offset, uint32_t data_count, char *data)
{
	assert(local_sandbox_context_cache.memory.size >= data_count);
	assert(offset < local_sandbox_context_cache.memory.size - data_count);

	memcpy(get_memory_ptr_for_runtime(offset, data_count), data, data_count);
}

/* If we are using runtime globals, we need to populate them */
WEAK void
populate_globals()
{
	assert(0); /* FIXME: is this used in WASM as dynamic modules? Issue #105. */
}
