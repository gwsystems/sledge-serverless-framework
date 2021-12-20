#include <threads.h>

#include "types.h"
#include "current_wasm_module_instance.h"
#include "wasm_memory.h"

INLINE void
check_bounds(uint32_t offset, uint32_t bounds_check)
{
	// Due to how we setup memory for x86, the virtual memory mechanism will catch the error, if bounds <
	// WASM_PAGE_SIZE
	assert(bounds_check < WASM_PAGE_SIZE
	       || (current_wasm_module_instance.memory.size > bounds_check
	           && offset <= current_wasm_module_instance.memory.size - bounds_check));
}

/**
 * @brief Get the memory ptr for runtime object
 *
 * @param offset base offset of pointer
 * @param length length starting at base offset
 * @return host address of offset into WebAssembly linear memory
 */
INLINE char *
get_memory_ptr_for_runtime(uint32_t offset, uint32_t length)
{
	if (unlikely(offset + length > current_wasm_module_instance.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %lu\n", offset, length,
		        current_wasm_module_instance.memory.size);
		current_wasm_module_instance_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)current_wasm_module_instance.memory.buffer;
	char *address      = &mem_as_chars[offset];
	return address;
}

extern thread_local struct wasm_module_instance current_wasm_module_instance;
#define CURRENT_MEMORY_BASE  current_wasm_module_instance.memory.buffer
#define CURRENT_MEMORY_SIZE  current_wasm_module_instance.memory.size
#define CURRENT_WASI_CONTEXT current_wasm_module_instance.wasi_context


#include "wasi_backing.h"
