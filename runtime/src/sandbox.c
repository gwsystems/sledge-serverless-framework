#include <assert.h>
#include <sys/mman.h>

#include "current_sandbox.h"
#include "debuglog.h"
#include "panic.h"
#include "sandbox_functions.h"
#include "sandbox_set_as_error.h"
#include "sandbox_set_as_initialized.h"

/**
 * Allocates a WebAssembly sandbox represented by the following layout
 * struct sandbox | HTTP Req Buffer | HTTP Resp Buffer | 4GB of Wasm Linear Memory | Guard Page
 * @param module the module that we want to run
 * @returns the resulting sandbox or NULL if mmap failed
 */
static inline struct sandbox *
sandbox_allocate_memory(struct module *module)
{
	assert(module != NULL);

	char *          error_message             = NULL;
	unsigned long   memory_size               = WASM_PAGE_SIZE * WASM_MEMORY_PAGES_INITIAL; /* The initial pages */
	uint64_t        memory_max                = (uint64_t)WASM_PAGE_SIZE * WASM_MEMORY_PAGES_MAX;
	struct sandbox *sandbox                   = NULL;
	unsigned long   page_aligned_sandbox_size = round_up_to_page(sizeof(struct sandbox));

	unsigned long size_to_alloc = page_aligned_sandbox_size + module->max_request_size + module->max_request_size
	                              + memory_max + /* guard page */ PAGE_SIZE;
	unsigned long size_to_read_write = page_aligned_sandbox_size + module->max_request_size
	                                   + module->max_request_size + memory_size;

	/*
	 * Control information should be page-aligned
	 */
	assert(round_up_to_page(size_to_alloc) == size_to_alloc);

	/* At an address of the system's choosing, allocate the memory, marking it as inaccessible */
	errno      = 0;
	void *addr = mmap(NULL, size_to_alloc, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		error_message = "sandbox_allocate_memory - memory allocation failed";
		goto alloc_failed;
	}

	assert(addr != NULL);

	/* Set the struct sandbox, HTTP Req/Resp buffer, and the initial Wasm Pages as read/write */
	errno         = 0;
	void *addr_rw = mmap(addr, size_to_read_write, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
	                     -1, 0);
	if (addr_rw == MAP_FAILED) {
		error_message = "set to r/w";
		goto set_rw_failed;
	}

	sandbox = (struct sandbox *)addr_rw;

	/* Populate Sandbox members */
	sandbox->state  = SANDBOX_UNINITIALIZED;
	sandbox->module = module;
	module_acquire(module);

	sandbox->request.base   = (char *)addr + page_aligned_sandbox_size;
	sandbox->request.length = 0;

	sandbox->response.base  = (char *)addr + page_aligned_sandbox_size + module->max_request_size;
	sandbox->request.length = 0;

	sandbox->memory.start = (char *)addr + page_aligned_sandbox_size + module->max_request_size
	                        + module->max_request_size;
	sandbox->memory.size = memory_size;
	sandbox->memory.max  = memory_max;

done:
	return sandbox;
set_rw_failed:
	sandbox = NULL;
	errno   = 0;
	int rc  = munmap(addr, size_to_alloc);
	if (rc == -1) perror("Failed to munmap after fail to set r/w");
alloc_failed:
err:
	perror(error_message);
	goto done;
}

static inline int
sandbox_allocate_stack(struct sandbox *sandbox)
{
	assert(sandbox);
	assert(sandbox->module);

	errno      = 0;
	char *addr = mmap(NULL, sandbox->module->stack_size + /* guard page */ PAGE_SIZE, PROT_NONE,
	                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) goto err_stack_allocation_failed;

	/* Set the struct sandbox, HTTP Req/Resp buffer, and the initial Wasm Pages as read/write */
	errno         = 0;
	char *addr_rw = mmap(addr + /* guard page */ PAGE_SIZE, sandbox->module->stack_size, PROT_READ | PROT_WRITE,
	                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	/* TODO: Fix leak here. Issue #132 */
	if (addr_rw == MAP_FAILED) goto err_stack_allocation_failed;

	sandbox->stack.start = addr_rw;
	sandbox->stack.size  = sandbox->module->stack_size;

done:
	return 0;
err_stack_allocation_failed:
	perror("sandbox_allocate_stack");
	return -1;
}

/**
 * Allocates a new sandbox from a sandbox request
 * Frees the sandbox request on success
 * @param sandbox_request request being allocated
 * @returns sandbox * on success, NULL on error
 */
struct sandbox *
sandbox_allocate(struct sandbox_request *sandbox_request)
{
	/* Validate Arguments */
	assert(sandbox_request != NULL);

	struct sandbox *sandbox;
	char *          error_message = "";
	uint64_t        now           = __getcycles();

	/* Allocate Sandbox control structures, buffers, and linear memory in a 4GB address space */
	sandbox = sandbox_allocate_memory(sandbox_request->module);
	if (!sandbox) {
		error_message = "failed to allocate sandbox heap and linear memory";
		goto err_memory_allocation_failed;
	}

	/* Allocate the Stack */
	if (sandbox_allocate_stack(sandbox) < 0) {
		error_message = "failed to allocate sandbox stack";
		goto err_stack_allocation_failed;
	}
	sandbox->state = SANDBOX_ALLOCATED;

	/* Set state to initializing */
	sandbox_set_as_initialized(sandbox, sandbox_request, now);

	free(sandbox_request);
done:
	return sandbox;
err_stack_allocation_failed:
	/*
	 * This is a degenerate sandbox that never successfully completed initialization, so we need to
	 * hand jam some things to be able to cleanly transition to ERROR state
	 */
	sandbox->state                          = SANDBOX_SET_AS_INITIALIZED;
	sandbox->timestamp_of.last_state_change = now;
#ifdef LOG_SANDBOX_MEMORY_PROFILE
	sandbox->timestamp_of.page_allocations_size = 0;
#endif
	ps_list_init_d(sandbox);
err_memory_allocation_failed:
	sandbox_set_as_error(sandbox, SANDBOX_SET_AS_INITIALIZED);
	perror(error_message);
	sandbox = NULL;
	goto done;
}


/**
 * Free stack and heap resources.. also any I/O handles.
 * @param sandbox
 */
void
sandbox_free(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox != current_sandbox_get());
	assert(sandbox->state == SANDBOX_ERROR || sandbox->state == SANDBOX_COMPLETE);

	int rc;

	module_release(sandbox->module);

	/* Free Sandbox Stack */
	errno = 0;

	/* The stack start is the bottom of the usable stack, but we allocated a guard page below this */
	rc = munmap((char *)sandbox->stack.start - PAGE_SIZE, sandbox->stack.size + PAGE_SIZE);
	if (rc == -1) {
		debuglog("Failed to unmap stack of Sandbox %lu\n", sandbox->id);
		goto err_free_stack_failed;
	};


	/* Free Sandbox Struct and HTTP Request and Response Buffers
	 * The linear memory was already freed during the transition from running to error|complete
	 * struct sandbox | HTTP Request Buffer | HTTP Response Buffer | 4GB of Wasm Linear Memory | Guard Page
	 * Allocated      | Allocated           | Allocated            | Freed                     | Freed
	 */

	/* Linear Memory and Guard Page should already have been munmaped and set to NULL */
	assert(sandbox->memory.start == NULL);
	errno = 0;

	unsigned long size_to_unmap = round_up_to_page(sizeof(struct sandbox)) + sandbox->module->max_request_size
	                              + sandbox->module->max_response_size;
	munmap(sandbox, size_to_unmap);
	if (rc == -1) {
		debuglog("Failed to unmap Sandbox %lu\n", sandbox->id);
		goto err_free_sandbox_failed;
	};

done:
	return;
err_free_sandbox_failed:
err_free_stack_failed:
	/* Errors freeing memory is a fatal error */
	panic("Failed to free Sandbox %lu\n", sandbox->id);
}
