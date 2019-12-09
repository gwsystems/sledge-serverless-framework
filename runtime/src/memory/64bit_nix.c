/* Code from https://github.com/gwsystems/silverfish/blob/master/runtime/memory/64bit_nix.c */
#include <runtime.h>
#include <sandbox.h>

#ifdef USE_MEM_VM

#include <sys/mman.h>

#define MAX_LINEAR_MEM ((1LL << 32) + WASM_PAGE_SIZE)

void
alloc_linear_memory(void)
{
	struct sandbox *curr = sandbox_current();

	// Map 4gb + PAGE_SIZE of memory that will fault when accessed
	// We allocate the extra page so that reads off the end will also fail
	sandbox_lmbase = mmap(NULL, MAX_LINEAR_MEM, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (sandbox_lmbase == MAP_FAILED) {
		perror("Mapping of initial unusable region failed");
		exit(1);
	}

	void *map_result = mmap(sandbox_lmbase, WASM_PAGE_SIZE * WASM_START_PAGES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (map_result == MAP_FAILED) {
		perror("Mapping of initial memory failed");
		exit(1);
	}
	sandbox_lmbound = WASM_PAGE_SIZE * WASM_START_PAGES;

	curr->linear_start = sandbox_lmbase;
	curr->linear_size  = sandbox_lmbound;
}

void
free_linear_memory(void *base, u32 bound, u32 max)
{
	struct sandbox *curr = sandbox_current();

	assert(base && bound);

	// cannot free currently executing sandbox's memory
	assert(curr == NULL || base != curr->linear_start || base != sandbox_lmbase);

	int ret = munmap(base, MAX_LINEAR_MEM);
	if (ret) perror("munmap");
}

void
expand_memory(void)
{
	struct sandbox *curr = sandbox_current();

	// max_pages = 0 => no limit: FIXME
	assert(sandbox_lmbound / WASM_PAGE_SIZE < WASM_MAX_PAGES);
	// Remap the relevant wasm page to readable
	char *mem_as_chars = sandbox_lmbase;
	char *page_address = &mem_as_chars[sandbox_lmbound];

	void *map_result = mmap(page_address, WASM_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (map_result == MAP_FAILED) {
		perror("Mapping of new memory failed");
		exit(1);
	}

	// TODO: check curr->linear_max_size
	sandbox_lmbound += WASM_PAGE_SIZE;
	curr->linear_size = sandbox_lmbound;
}

INLINE char *
get_memory_ptr_for_runtime(u32 offset, u32 bounds_check)
{
	// Due to how we setup memory for x86, the virtual memory mechanism will catch the error, if bounds < WASM_PAGE_SIZE
	assert(bounds_check < WASM_PAGE_SIZE || 
	       (sandbox_lmbound > bounds_check && 
		offset <= sandbox_lmbound - bounds_check));

	char *mem_as_chars = (char *)sandbox_lmbase;
	char *address = &mem_as_chars[offset];

	return address;
}

#else
#error "Incorrect runtime memory module!"
#endif
