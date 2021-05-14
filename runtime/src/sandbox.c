#include <assert.h>
#include <sys/mman.h>

#include "current_sandbox.h"
#include "debuglog.h"
#include "panic.h"
#include "sandbox_functions.h"
#include "sandbox_set_as_error.h"
#include "sandbox_set_as_initialized.h"

/**
 * Close the sandbox's ith io_handle
 * @param sandbox
 * @param sandbox_fd client fd to close
 */
// void
// sandbox_close_file_descriptor(struct sandbox *sandbox, int sandbox_fd)
// {
// 	if (sandbox_fd >= SANDBOX_MAX_FD_COUNT || sandbox_fd < 0) return;
// 	/* TODO: Do we actually need to call some sort of close function here? Issue #90 */
// 	/* Thought: do we need to refcount host fds? */
// 	sandbox->file_descriptors[sandbox_fd] = -1;
// }

/**
 * Allocates a WebAssembly sandbox represented by the following layout
 * struct sandbox | Buffer for HTTP Req/Resp | 4GB of Wasm Linear Memory | Guard Page
 * @param module the module that we want to run
 * @returns the resulting sandbox or NULL if mmap failed
 */
static inline struct sandbox *
sandbox_allocate_memory(struct module *module)
{
	assert(module != NULL);

	char *          error_message          = NULL;
	unsigned long   linear_memory_size     = WASM_PAGE_SIZE * WASM_START_PAGES; /* The initial pages */
	uint64_t        linear_memory_max_size = (uint64_t)SANDBOX_MAX_MEMORY;
	struct sandbox *sandbox                = NULL;
	unsigned long   sandbox_size           = sizeof(struct sandbox) + module->max_request_or_response_size;

	/*
	 * Control information should be page-aligned
	 * TODO: Should I use round_up_to_page when setting sandbox_page? Issue #50
	 */
	assert(round_up_to_page(sandbox_size) == sandbox_size);

	/* At an address of the system's choosing, allocate the memory, marking it as inaccessible */
	errno      = 0;
	void *addr = mmap(NULL, sandbox_size + linear_memory_max_size + /* guard page */ PAGE_SIZE, PROT_NONE,
	                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		error_message = "sandbox_allocate_memory - memory allocation failed";
		goto alloc_failed;
	}

	assert(addr != NULL);

	/* Set the struct sandbox, HTTP Req/Resp buffer, and the initial Wasm Pages as read/write */
	errno         = 0;
	void *addr_rw = mmap(addr, sandbox_size + linear_memory_size, PROT_READ | PROT_WRITE,
	                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (addr_rw == MAP_FAILED) {
		error_message = "set to r/w";
		goto set_rw_failed;
	}

	sandbox = (struct sandbox *)addr_rw;

	/* Populate Sandbox members */
	sandbox->state                  = SANDBOX_UNINITIALIZED;
	sandbox->linear_memory_start    = (char *)addr + sandbox_size;
	sandbox->linear_memory_size     = linear_memory_size;
	sandbox->linear_memory_max_size = linear_memory_max_size;
	sandbox->module                 = module;
	sandbox->sandbox_size           = sandbox_size;
	module_acquire(module);

done:
	return sandbox;
set_rw_failed:
	sandbox = NULL;
	errno   = 0;
	int rc  = munmap(addr, sandbox_size + linear_memory_size + PAGE_SIZE);
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

	sandbox->stack_start = addr_rw;
	sandbox->stack_size  = sandbox->module->stack_size;

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
	module_validate(sandbox_request->module);

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
	sandbox->state                       = SANDBOX_SET_AS_INITIALIZED;
	sandbox->last_state_change_timestamp = now;
#ifdef LOG_SANDBOX_MEMORY_PROFILE
	sandbox->page_allocation_timestamps_size = 0;
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
	rc = munmap((char *)sandbox->stack_start - PAGE_SIZE, sandbox->stack_size + PAGE_SIZE);
	if (rc == -1) {
		debuglog("Failed to unmap stack of Sandbox %lu\n", sandbox->id);
		goto err_free_stack_failed;
	};


	/* Free Remaining Sandbox Linear Address Space
	 * sandbox_size includes the struct and HTTP buffer
	 * The linear memory was already freed during the transition from running to error|complete
	 * struct sandbox | HTTP Buffer | 4GB of Wasm Linear Memory | Guard Page
	 * Allocated      | Allocated   | Freed                     | Freed
	 */

	/* Linear Memory and Guard Page should already have been munmaped and set to NULL */
	assert(sandbox->linear_memory_start == NULL);
	errno = 0;
	rc    = munmap(sandbox, sandbox->sandbox_size);
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
