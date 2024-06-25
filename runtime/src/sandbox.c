#include <assert.h>
#include <string.h>
#include <sys/mman.h>

#include "sandbox_types.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "panic.h"
#include "pool.h"
#include "runtime.h"
#include "sandbox_functions.h"
#include "sandbox_set_as_allocated.h"
#include "sandbox_set_as_error.h"
#include "sandbox_set_as_initialized.h"
#include "sandbox_total.h"
#include "wasm_memory.h"
#include "wasm_stack.h"

_Atomic uint64_t sandbox_total = 0;

static inline void
sandbox_log_allocation(struct sandbox *sandbox)
{
#ifdef LOG_SANDBOX_ALLOCATION
	debuglog("Sandbox %lu: of %s %s\n", sandbox->id, sandbox->tenant->name, sandbox->route->route);
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

	// sandbox->sizes[sandbox->sizesize] = sandbox->memory->abi.size;
	// sandbox->sizesize++;
	sandbox->memory->abi.id = sandbox->id;

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

	return module_free_stack(sandbox->module, sandbox->stack, sandbox->original_owner_worker_idx);
}

// /**
//  * Free Linear Memory, leaving stack in place
//  * @param sandbox
//  */
// static inline void
// sandbox_free_linear_memory(struct sandbox *sandbox)
// {
// 	assert(sandbox != NULL);
// 	assert(sandbox->memory != NULL);
// 	module_free_linear_memory(sandbox->module, (struct wasm_memory *)sandbox->memory);
// 	sandbox->memory = NULL;
// }

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

	rc = http_session_init_response_body(sandbox->http);
	if (rc < 0) {
		error_message = "failed to allocate response body";
		goto err_globals_allocation_failed;
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
	sandbox_set_as_error(sandbox, SANDBOX_INITIALIZED);
	sandbox_free(sandbox);
	perror(error_message);
	rc = -1;
	goto done;
}

void
sandbox_init(struct sandbox *sandbox, struct module *module, struct http_session *session, uint64_t admissions_estimate)
{
	/* Sets the ID to the value before the increment */
	sandbox->id     = sandbox_total_postfix_increment();
	sandbox->module = module;
	module_acquire(sandbox->module);

	/* Initialize Parsec control structures */
	ps_list_init_d(sandbox);

	/* Allocate HTTP session structure */
	assert(session);
	sandbox->http   = session;
	sandbox->tenant = session->tenant;
	sandbox->route  = session->route;

	sandbox->absolute_deadline = sandbox->timestamp_of.allocation + sandbox->route->relative_deadline;
	sandbox->payload_size      = session->http_request.body_length;

	sandbox->exceeded_estimation = false;
	sandbox->writeback_preemption_in_progress = false;
	sandbox->writeback_overshoot_in_progress = false;
	sandbox->response_code       = 0;
	sandbox->owned_worker_idx    = -2;
	sandbox->original_owner_worker_idx = -2;

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
 * @param admissions_estimate the timestamp of when we receives the request from the network (in cycles)
 * @return the new sandbox request
 */
struct sandbox *
sandbox_alloc(struct module *module, struct http_session *session, uint64_t admissions_estimate, uint64_t sandbox_alloc_timestamp)
{
	size_t alignment     = (size_t)PAGE_SIZE;
	size_t size_to_alloc = (size_t)round_up_to_page(sizeof(struct sandbox));

	assert(size_to_alloc % alignment == 0);

	struct sandbox *sandbox = NULL;
	sandbox                 = aligned_alloc(alignment, size_to_alloc);

	if (unlikely(sandbox == NULL)) return NULL;
	memset(sandbox, 0, size_to_alloc);

	sandbox->timestamp_of.allocation = sandbox_alloc_timestamp;
	sandbox_set_as_allocated(sandbox);
	sandbox_init(sandbox, module, session, admissions_estimate);

	sandbox_refs[sandbox->id % RUNTIME_MAX_ALIVE_SANDBOXES] = true;

	return sandbox;
}

struct sandbox_metadata *sandbox_meta_alloc(struct sandbox *sandbox)
{
	struct sandbox_metadata *sandbox_meta = malloc(sizeof(struct sandbox_metadata));

	sandbox_meta->sandbox_shadow       = sandbox;
	sandbox_meta->tenant               = sandbox->tenant;
	sandbox_meta->route                = sandbox->route;
	sandbox_meta->id                   = sandbox->id;
	sandbox_meta->state                = sandbox->state;
	sandbox_meta->allocation_timestamp = sandbox->timestamp_of.allocation;
	sandbox_meta->absolute_deadline    = sandbox->absolute_deadline;
	sandbox_meta->remaining_exec       = sandbox->remaining_exec;
	sandbox_meta->exceeded_estimation  = sandbox->exceeded_estimation;
	sandbox_meta->owned_worker_idx     = worker_thread_idx;
	sandbox_meta->terminated           = false;
	sandbox_meta->pq_idx_in_tenant_queue = 0;

	return sandbox_meta;
}

void
sandbox_deinit(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox != current_sandbox_get());
	assert(sandbox->state == SANDBOX_ERROR || sandbox->state == SANDBOX_COMPLETE);

	/* Assumption: HTTP session was migrated to listener core */
	assert(sandbox->http == NULL);

	module_release(sandbox->module);

	/* Linear Memory and Guard Page should already have been munmaped and set to NULL */
	// assert(sandbox->memory == NULL);

	if (likely(sandbox->memory != NULL)) sandbox_free_linear_memory(sandbox);
	if (likely(sandbox->stack != NULL)) sandbox_free_stack(sandbox);
	if (likely(sandbox->globals.buffer != NULL)) sandbox_free_globals(sandbox);
	if (likely(sandbox->wasi_context != NULL)) wasi_context_destroy(sandbox->wasi_context);
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
	assert(!listener_thread_is_running() || sandbox->memory == NULL);

	sandbox_refs[sandbox->id % RUNTIME_MAX_ALIVE_SANDBOXES] = false;
	sandbox_deinit(sandbox);
	free(sandbox);
}
