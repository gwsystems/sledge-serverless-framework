#include <assert.h>
#include <string.h>
#include <sys/mman.h>

#include "current_sandbox.h"
#include "debuglog.h"
#include "panic.h"
#include "pool.h"
#include "sandbox_functions.h"
#include "sandbox_set_as_error.h"
#include "sandbox_set_as_initialized.h"
#include "buffer.h"

/**
 * Allocates a WebAssembly sandbox represented by the following layout
 * struct sandbox | HTTP Req Buffer | HTTP Resp Buffer | 4GB of Wasm Linear Memory | Guard Page
 * @param module the module that we want to run
 * @returns the resulting sandbox or NULL if mmap failed
 */
static inline int
sandbox_allocate_linear_memory(struct sandbox *self)
{
	assert(self != NULL);

	char *   error_message = NULL;
	uint64_t memory_max    = (uint64_t)WASM_PAGE_SIZE * WASM_MEMORY_PAGES_MAX;

	struct buffer *linear_memory = (struct buffer *)pool_allocate_object(
	  self->module->linear_memory_pool[worker_thread_idx]);

	size_t initial = (size_t)self->module->abi.starting_pages * WASM_PAGE_SIZE;
	size_t max     = (size_t)WASM_MEMORY_PAGES_MAX * WASM_PAGE_SIZE;

	if (linear_memory == NULL) {
		linear_memory = buffer_allocate(initial, max);
		if (unlikely(linear_memory == NULL)) return -1;
	}

	self->memory = linear_memory;
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

	struct sandbox *self                      = NULL;
	size_t          page_aligned_sandbox_size = round_up_to_page(sizeof(struct sandbox));
	self                                      = calloc(1, page_aligned_sandbox_size);
	if (self == NULL) goto err_struct_allocation_failed;

	/* Set state to initializing */
	sandbox_set_as_initialized(self, sandbox_request, now);

	if (sandbox_allocate_http_buffers(self)) {
		error_message = "failed to allocate http buffers";
		goto err_http_allocation_failed;
	}

	/* Allocate linear memory in a 4GB address space */
	if (sandbox_allocate_linear_memory(self)) {
		error_message = "failed to allocate sandbox linear memory";
		goto err_memory_allocation_failed;
	}

	/* Allocate the Stack */
	if (sandbox_allocate_stack(self) < 0) {
		error_message = "failed to allocate sandbox stack";
		goto err_stack_allocation_failed;
	}

	/* Initialize the sandbox's context, stack, and instruction pointer */
	/* stack.start points to the bottom of the usable stack, so add stack_size to get to top */
	arch_context_init(&self->ctxt, (reg_t)current_sandbox_start, (reg_t)self->stack.start + self->stack.size);

	free(sandbox_request);
done:
	return self;
err_stack_allocation_failed:
	/*
	 * This is a degenerate sandbox that never successfully completed initialization, so we need to
	 * hand jam some things to be able to cleanly transition to ERROR state
	 */
	self->state                          = SANDBOX_UNINITIALIZED;
	self->timestamp_of.last_state_change = now;

	ps_list_init_d(self);
err_memory_allocation_failed:
err_http_allocation_failed:
	sandbox_set_as_error(self, SANDBOX_UNINITIALIZED);
	perror(error_message);
err_struct_allocation_failed:
	self = NULL;
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

	free(sandbox->request.base);
	free(sandbox->response.base);
	free(sandbox);
done:
	return;
err_free_sandbox_failed:
err_free_stack_failed:
	/* Errors freeing memory is a fatal error */
	panic("Failed to free Sandbox %lu\n", sandbox->id);
}
