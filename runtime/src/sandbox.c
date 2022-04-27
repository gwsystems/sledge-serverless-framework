#include <assert.h>
#include <string.h>
#include <sys/mman.h>

#include "current_sandbox.h"
#include "debuglog.h"
#include "panic.h"
#include "pool.h"
#include "runtime.h"
#include "sandbox_functions.h"
#include "sandbox_set_as_error.h"
#include "sandbox_set_as_initialized.h"
#include "sandbox_set_as_allocated.h"
#include "sandbox_total.h"
#include "wasm_memory.h"
#include "wasm_stack.h"

_Atomic uint32_t sandbox_total = 0;

static inline void
sandbox_log_allocation(struct sandbox *sandbox)
{
#ifdef LOG_SANDBOX_ALLOCATION
	debuglog("Sandbox %lu: of %s:%d\n", sandbox->id, sandbox->module->name, sandbox->module->port);
#endif
}

/**
 * Allocates a WebAssembly linear memory for a sandbox based on the starting_pages and max_pages globals present in
 * the associated *.so module
 * @param sandbox sandbox that we want to allocate a linear memory for
 * @returns 0 on success, -1 on error
 */
static inline int
sandbox_allocate_linear_memory(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	sandbox->memory = module_allocate_linear_memory(sandbox->module);
	if (unlikely(sandbox->memory == NULL)) return -1;

	return 0;
}

static inline int
sandbox_allocate_stack(struct sandbox *sandbox)
{
	assert(sandbox);
	assert(sandbox->module);

	sandbox->stack = module_allocate_stack(sandbox->module);
	if (sandbox->stack == NULL) return -1;

	return 0;
}

static inline int
sandbox_allocate_globals(struct sandbox *sandbox)
{
	assert(sandbox);
	assert(sandbox->module);

	return wasm_globals_init(&sandbox->globals, sandbox->module->abi.globals_len);
}

static inline void
sandbox_free_globals(struct sandbox *sandbox)
{
	assert(sandbox);
	assert(sandbox->module);
	assert(sandbox->globals.buffer != NULL);

	wasm_globals_deinit(&sandbox->globals);
}

static inline void
sandbox_free_stack(struct sandbox *sandbox)
{
	assert(sandbox);

	return module_free_stack(sandbox->module, sandbox->stack);
}

/**
 * Allocate http request and response buffers for a sandbox
 * @param sandbox sandbox that we want to allocate HTTP buffers for
 * @returns 0 on success, -1 on error
 */
static inline int
sandbox_allocate_http_buffers(struct sandbox *sandbox)
{
	int rc;
	rc = vec_u8_init(&sandbox->request, sandbox->module->max_request_size);
	if (rc < 0) return -1;

	rc = vec_u8_init(&sandbox->response, sandbox->module->max_response_size);
	if (rc < 0) {
		vec_u8_deinit(&sandbox->request);
		return -1;
	}

	return 0;
}


/**
 * Allocates HTTP buffers and performs our approximation of "WebAssembly instantiation"
 * @param sandbox
 * @returns 0 on success, -1 on error
 */
int
sandbox_prepare_execution_environment(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	char *error_message = "";

	int rc;

	if (sandbox_allocate_http_buffers(sandbox)) {
		error_message = "failed to allocate http buffers";
		goto err_http_allocation_failed;
	}

	rc = sandbox_allocate_globals(sandbox);
	if (rc < 0) {
		error_message = "failed to allocate globals";
		goto err_globals_allocation_failed;
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

	/* Initialize the sandbox's context, stack, and instruction pointer */
	/* stack grows down, so set to high address */
	arch_context_init(&sandbox->ctxt, (reg_t)current_sandbox_start, (reg_t)sandbox->stack->high);

	rc = 0;
done:
	return rc;
err_stack_allocation_failed:
err_memory_allocation_failed:
err_globals_allocation_failed:
err_http_allocation_failed:
	client_socket_send_oneshot(sandbox->client_socket_descriptor, http_header_build(503), http_header_len(503));
	client_socket_close(sandbox->client_socket_descriptor, &sandbox->client_address);
	sandbox_set_as_error(sandbox, SANDBOX_ALLOCATED);
	perror(error_message);
	rc = -1;
	goto done;
}

void
sandbox_init(struct sandbox *sandbox, struct module *module, int socket_descriptor,
             const struct sockaddr *socket_address, uint64_t request_arrival_timestamp, uint64_t admissions_estimate)
{
	/* Sets the ID to the value before the increment */
	sandbox->id     = sandbox_total_postfix_increment();
	sandbox->module = module;
	module_acquire(sandbox->module);

	/* Initialize Parsec control structures */
	ps_list_init_d(sandbox);

	sandbox->client_socket_descriptor = socket_descriptor;
	memcpy(&sandbox->client_address, socket_address, sizeof(struct sockaddr));
	sandbox->timestamp_of.request_arrival = request_arrival_timestamp;
	sandbox->absolute_deadline            = request_arrival_timestamp + module->relative_deadline;

	/*
	 * Admissions Control State
	 * Assumption: an estimate of 0 should have been interpreted as a rejection
	 */
	assert(admissions_estimate != 0);
	sandbox->admissions_estimate = admissions_estimate;

	sandbox_log_allocation(sandbox);
	sandbox_set_as_initialized(sandbox, SANDBOX_ALLOCATED);
}

/**
 * Allocates a new Sandbox Request and places it on the Global Deque
 * @param module the module we want to request
 * @param socket_descriptor
 * @param socket_address
 * @param request_arrival_timestamp the timestamp of when we receives the request from the network (in cycles)
 * @param admissions_estimate the timestamp of when we receives the request from the network (in cycles)
 * @return the new sandbox request
 */
struct sandbox *
sandbox_alloc(struct module *module, int socket_descriptor, const struct sockaddr *socket_address,
              uint64_t request_arrival_timestamp, uint64_t admissions_estimate)
{
	struct sandbox *sandbox                   = NULL;
	size_t          page_aligned_sandbox_size = round_up_to_page(sizeof(struct sandbox));
	sandbox                                   = aligned_alloc(PAGE_SIZE, page_aligned_sandbox_size);
	memset(sandbox, 0, page_aligned_sandbox_size);
	if (unlikely(sandbox == NULL)) return NULL;

	sandbox_set_as_allocated(sandbox);
	sandbox_init(sandbox, module, socket_descriptor, socket_address, request_arrival_timestamp,
	             admissions_estimate);


	return sandbox;
}

void
sandbox_deinit(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox != current_sandbox_get());
	assert(sandbox->state == SANDBOX_ERROR || sandbox->state == SANDBOX_COMPLETE);

	module_release(sandbox->module);

	/* Linear Memory and Guard Page should already have been munmaped and set to NULL */
	assert(sandbox->memory == NULL);

	if (likely(sandbox->stack != NULL)) sandbox_free_stack(sandbox);

	if (likely(sandbox->globals.buffer != NULL)) sandbox_free_globals(sandbox);
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

	sandbox_deinit(sandbox);
	free(sandbox);
}
