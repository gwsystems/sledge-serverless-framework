#include <threads.h>

#include "current_wasm_module_instance.h"
#include "sledge_abi.h"
#include "types.h"
#include "wasm_memory.h"

INLINE void
check_bounds(uint32_t offset, uint32_t bounds_check)
{
	// Due to how we setup memory for x86, the virtual memory mechanism will catch the error, if bounds <
	// WASM_PAGE_SIZE
	assert(bounds_check < WASM_PAGE_SIZE
	       || (sledge_abi__current_wasm_module_instance.abi.memory.size > bounds_check
	           && offset <= sledge_abi__current_wasm_module_instance.abi.memory.size - bounds_check));
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
	assert((uint64_t)offset + length < sledge_abi__current_wasm_module_instance.abi.memory.size);
	char *mem_as_chars = (char *)sledge_abi__current_wasm_module_instance.abi.memory.buffer;
	char *address      = &mem_as_chars[offset];
	return address;
}

extern thread_local struct wasm_module_instance sledge_abi__current_wasm_module_instance;
#define CURRENT_MEMORY_BASE  sledge_abi__current_wasm_module_instance.abi.memory.buffer
#define CURRENT_MEMORY_SIZE  sledge_abi__current_wasm_module_instance.abi.memory.size
#define CURRENT_WASI_CONTEXT sledge_abi__current_wasm_module_instance.wasi_context


#include "wasi_backing.h"
