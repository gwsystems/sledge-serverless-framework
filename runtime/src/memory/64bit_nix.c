/* Code from https://github.com/gwsystems/silverfish/blob/master/runtime/memory/64bit_nix.c */
#include "current_sandbox.h"
#include "panic.h"
#include "runtime.h"
#include "sandbox.h"
#include "types.h"

#ifdef USE_MEM_VM

#include <sys/mman.h>

#define MAX_LINEAR_MEM ((1LL << 32) + WASM_PAGE_SIZE)

void
alloc_linear_memory(void)
{
	// mmaped memory in sandbox_allocate.
}

void
free_linear_memory(void *base, u32 bound, u32 max)
{
	// frees on sandbox_free
}

void
expand_memory(void)
{
	struct sandbox *sandbox = current_sandbox_get();

	// max_pages = 0 => no limit: FIXME
	assert((sandbox->sandbox_size + local_sandbox_member_cache.linear_memory_size) / WASM_PAGE_SIZE
	       < WASM_MAX_PAGES);
	// Remap the relevant wasm page to readable
	char *mem_as_chars = local_sandbox_member_cache.linear_memory_start;
	char *page_address = &mem_as_chars[local_sandbox_member_cache.linear_memory_size];

	void *map_result = mmap(page_address, WASM_PAGE_SIZE, PROT_READ | PROT_WRITE,
	                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	// TODO: Refactor to return RC signifying out-of-mem to caller
	if (map_result == MAP_FAILED) panic("Mapping of new memory failed");
	if (local_sandbox_member_cache.linear_memory_size > sandbox->linear_memory_max_size)
		panic("expand_memory - Out of Memory!\n");

	local_sandbox_member_cache.linear_memory_size += WASM_PAGE_SIZE;
	// local_sandbox_member_cache is "forked state", so update authoritative member
	sandbox->linear_memory_size = local_sandbox_member_cache.linear_memory_size;
}

INLINE char *
get_memory_ptr_for_runtime(u32 offset, u32 bounds_check)
{
	// Due to how we setup memory for x86, the virtual memory mechanism will catch the error, if bounds <
	// WASM_PAGE_SIZE
	assert(bounds_check < WASM_PAGE_SIZE
	       || (local_sandbox_member_cache.linear_memory_size > bounds_check
	           && offset <= local_sandbox_member_cache.linear_memory_size - bounds_check));

	char *mem_as_chars = (char *)local_sandbox_member_cache.linear_memory_start;
	char *address      = &mem_as_chars[offset];

	return address;
}

#else
#error "Incorrect runtime memory module!"
#endif
