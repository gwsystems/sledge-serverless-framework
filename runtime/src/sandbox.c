#include <assert.h>
#include <string.h>
#include <sys/mman.h>

#include "current_sandbox.h"
#include "debuglog.h"
#include "panic.h"
#include "sandbox_functions.h"
#include "sandbox_set_as_error.h"
#include "sandbox_set_as_initialized.h"
#include "wasm_memory.h"

/**
 * Allocates a WebAssembly sandbox represented by the following layout
 * struct sandbox | HTTP Req Buffer | HTTP Resp Buffer | 4GB of Wasm Linear Memory | Guard Page
 * @param module the module that we want to run
 * @returns the resulting sandbox or NULL if mmap failed
 */
static inline int
sandbox_allocate_linear_memory(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	char *error_message = NULL;

	size_t initial = (size_t)WASM_MEMORY_PAGES_INITIAL * WASM_PAGE_SIZE;
	size_t max     = (size_t)WASM_MEMORY_PAGES_MAX * WASM_PAGE_SIZE;

	assert(initial <= (size_t)UINT32_MAX + 1);
	assert(max <= (size_t)UINT32_MAX + 1);

	sandbox->memory = wasm_memory_allocate(initial, max);
	if (unlikely(sandbox->memory == NULL)) return -1;

	return 0;
}

static inline int
sandbox_allocate_stack(struct sandbox *sandbox)
{
	assert(sandbox);
	assert(sandbox->module);

	int rc = 0;

	char *addr = mmap(NULL, /* guard page */ PAGE_SIZE + sandbox->module->stack_size, PROT_NONE,
	                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (unlikely(addr == MAP_FAILED)) {
		perror("sandbox allocate stack");
		goto err_stack_allocation_failed;
	}

	/* Set the struct sandbox, HTTP Req/Resp buffer, and the initial Wasm Pages as read/write */
	char *addr_rw = mmap(addr + /* guard page */ PAGE_SIZE, sandbox->module->stack_size, PROT_READ | PROT_WRITE,
	                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (unlikely(addr_rw == MAP_FAILED)) {
		perror("sandbox set stack read/write");
		goto err_stack_allocation_failed;
	}

	sandbox->stack.start = addr_rw;
	sandbox->stack.size  = sandbox->module->stack_size;

	rc = 0;
done:
	return rc;
err_stack_prot_failed:
	rc = munmap(addr, sandbox->stack.size + PAGE_SIZE);
	if (rc == -1) perror("munmap");
err_stack_allocation_failed:
	sandbox->stack.start = NULL;
	sandbox->stack.size  = 0;
	goto done;
}

static inline int
sandbox_allocate_http_buffers(struct sandbox *self)
{
	self->request.base = calloc(1, self->module->max_request_size);
	if (self->request.base == NULL) return -1;
	self->request.length = 0;

	self->response.base = calloc(1, self->module->max_response_size);
	if (self->response.base == NULL) return -1;
	self->response.length = 0;

	return 0;
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

	char *   error_message = "";
	uint64_t now           = __getcycles();

	int rc;

	struct sandbox *sandbox                   = NULL;
	size_t          page_aligned_sandbox_size = round_up_to_page(sizeof(struct sandbox));
	sandbox                                   = calloc(1, page_aligned_sandbox_size);
	if (sandbox == NULL) goto err_struct_allocation_failed;

	/* Set state to initializing */
	sandbox_set_as_initialized(sandbox, sandbox_request, now);

	if (sandbox_allocate_http_buffers(sandbox)) {
		error_message = "failed to allocate http buffers";
		goto err_http_allocation_failed;
	}

	/* Allocate linear memory in a 4GB address space */
	if (sandbox_allocate_linear_memory(sandbox)) {
		error_message = "failed to allocate sandbox linear memory";
		goto err_memory_allocation_failed;
	}

	/* Allocate the Stack */
	if (sandbox_allocate_stack(sandbox) < 0) {
		error_message = "failed to allocate sandbox stack";
		goto err_stack_allocation_failed;
	}
	sandbox->state = SANDBOX_ALLOCATED;

#ifdef LOG_STATE_CHANGES
	sandbox->state_history_count                           = 0;
	sandbox->state_history[sandbox->state_history_count++] = SANDBOX_ALLOCATED;
	memset(&sandbox->state_history, 0, SANDBOX_STATE_HISTORY_CAPACITY * sizeof(sandbox_state_t));
#endif

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
	sandbox->state                          = SANDBOX_UNINITIALIZED;
	sandbox->timestamp_of.last_state_change = now;

	ps_list_init_d(sandbox);
err_memory_allocation_failed:
err_http_allocation_failed:
	sandbox_set_as_error(sandbox, SANDBOX_UNINITIALIZED);
	perror(error_message);
err_struct_allocation_failed:
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

	/* Free Sandbox Stack if initial allocation was successful */
	if (likely(sandbox->stack.size > 0)) {
		assert(sandbox->stack.start != NULL);
		/* The stack start is the bottom of the usable stack, but we allocated a guard page below this */
		rc = munmap((char *)sandbox->stack.start - PAGE_SIZE, sandbox->stack.size + PAGE_SIZE);
		if (unlikely(rc == -1)) {
			debuglog("Failed to unmap stack of Sandbox %lu\n", sandbox->id);
			goto err_free_stack_failed;
		};
	}


	/* Free Sandbox Struct and HTTP Request and Response Buffers
	 * The linear memory was already freed during the transition from running to error|complete
	 * struct sandbox | HTTP Request Buffer | HTTP Response Buffer | 4GB of Wasm Linear Memory | Guard Page
	 * Allocated      | Allocated           | Allocated            | Freed                     | Freed
	 */

	/* Linear Memory and Guard Page should already have been munmaped and set to NULL */
	assert(sandbox->memory->data == NULL);
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
