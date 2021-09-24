#include "current_sandbox.h"
#include "panic.h"
#include "runtime.h"
#include "sandbox_types.h"
#include "types.h"

#include <sys/mman.h>

/**
 * @brief Expand the linear memory of the active WebAssembly sandbox by a single page
 *
 * @return int
 */
int
expand_memory(void)
{
	struct sandbox *sandbox = current_sandbox_get();

	assert(sandbox->state == SANDBOX_RUNNING);
	assert(local_sandbox_context_cache.memory.size % WASM_PAGE_SIZE == 0);

	/* Return -1 if we've hit the linear memory max */
	if (unlikely(local_sandbox_context_cache.memory.size + WASM_PAGE_SIZE
	             >= local_sandbox_context_cache.memory.max)) {
		debuglog("expand_memory - Out of Memory!. %u out of %lu\n", local_sandbox_context_cache.memory.size,
		         local_sandbox_context_cache.memory.max);
		return -1;
	}

	// Remap the relevant wasm page to readable
	char *mem_as_chars = local_sandbox_context_cache.memory.start;
	char *page_address = &mem_as_chars[local_sandbox_context_cache.memory.size];
	void *map_result   = mmap(page_address, WASM_PAGE_SIZE, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (map_result == MAP_FAILED) {
		debuglog("Mapping of new memory failed");
		return -1;
	}

	local_sandbox_context_cache.memory.size += WASM_PAGE_SIZE;

#ifdef LOG_SANDBOX_MEMORY_PROFILE
	// Cache the runtime of the first N page allocations
	if (likely(sandbox->timestamp_of.page_allocations_size < SANDBOX_PAGE_ALLOCATION_TIMESTAMP_COUNT)) {
		sandbox->timestamp_of.page_allocations[sandbox->timestamp_of.page_allocations_size++] =
		  sandbox->duration_of_state.running
		  + (uint32_t)(__getcycles() - sandbox->timestamp_of.last_state_change);
	}
#endif

	// local_sandbox_context_cache is "forked state", so update authoritative member
	sandbox->memory.size = local_sandbox_context_cache.memory.size;
	return 0;
}

/**
 * @brief Get the memory ptr for runtime object
 *
 * @param offset base offset of pointer
 * @param length length starting at base offset
 * @return host address of offset into WebAssembly linear memory
 */
INLINE char *
get_memory_ptr(uint32_t offset, uint32_t length)
{
	if (unlikely(offset + length > local_sandbox_context_cache.memory.size)) {
		fprintf(stderr, "OOB: offset %u + length %u > size %u\n", offset, length,
		        local_sandbox_context_cache.memory.size);
		current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
	}

	char *mem_as_chars = (char *)local_sandbox_context_cache.memory.start;
	char *address      = &mem_as_chars[offset];

	return address;
}

/**
 * @brief Stub that implements the WebAssembly memory.grow instruction
 *
 * @param count number of pages to grow the WebAssembly linear memory by
 * @return The previous size of the linear memory in pages or -1 if enough memory cannot be allocated
 */
int32_t
instruction_memory_grow(uint32_t count)
{
	int rc = local_sandbox_context_cache.memory.size / WASM_PAGE_SIZE;

	for (int i = 0; i < count; i++) {
		if (unlikely(expand_memory() != 0)) {
			rc = -1;
			break;
		}
	}

	return rc;
}

/*
 * Table handling functionality
 * This was moved from compiletime in order to place the
 * function in the callstack in GDB. It can be moved back
 * to runtime/compiletime/memory/64bit_nix.c to remove the
 * additional function call
 */
char *
get_function_from_table(uint32_t idx, uint32_t type_id)
{
	if (unlikely(idx >= INDIRECT_TABLE_SIZE)) {
		fprintf(stderr, "idx: %u, Table size: %u\n", idx, INDIRECT_TABLE_SIZE);
		current_sandbox_trap(WASM_TRAP_INVALID_INDEX);
	}

	struct indirect_table_entry f = local_sandbox_context_cache.module_indirect_table[idx];
	if (unlikely(f.type_id != type_id)) {
		fprintf(stderr, "Function Type mismatch. Expected: %u, Actual: %u\n", type_id, f.type_id);
		current_sandbox_trap(WASM_TRAP_MISMATCHED_FUNCTION_TYPE);
	}
	if (unlikely(f.func_pointer == NULL)) {
		fprintf(stderr, "Function Type mismatch. Index %u resolved to NULL Pointer\n", idx);
		current_sandbox_trap(WASM_TRAP_MISMATCHED_FUNCTION_TYPE);
	}

	return f.func_pointer;
}
