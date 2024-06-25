#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>

#include "panic.h"
#include "sandbox_types.h"
#include "tenant.h"

/***************************
 * Public API              *
 **************************/

struct sandbox *sandbox_alloc(struct module *module, struct http_session *session, uint64_t admissions_estimate,
                              uint64_t sandbox_alloc_timestamp);
struct sandbox_metadata *sandbox_meta_alloc(struct sandbox *sandbox);
int                      sandbox_prepare_execution_environment(struct sandbox *sandbox);
void                     sandbox_free(struct sandbox *sandbox);
void                     sandbox_main(struct sandbox *sandbox);
void                     sandbox_switch_to(struct sandbox *next_sandbox);
void                     sandbox_process_scheduler_updates(struct sandbox *sandbox);

/**
 * Free Linear Memory, leaving stack in place
 * @param sandbox
 */
static inline void
sandbox_free_linear_memory(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox->memory != NULL);
	// if (worker_thread_idx != sandbox->original_owner_worker_idx)
	// 	printf("Me: %d, Orig: %d, Tenant: %s\n", worker_thread_idx, sandbox->original_owner_worker_idx, sandbox->tenant->name);
	module_free_linear_memory(sandbox->module, (struct wasm_memory *)sandbox->memory, sandbox->original_owner_worker_idx);
	sandbox->memory = NULL;
}

/**
 * Given a sandbox, returns the module that sandbox is executing
 * @param sandbox the sandbox whose module we want
 * @return the module of the provided sandbox
 */
static inline struct module *
sandbox_get_module(struct sandbox *sandbox)
{
	if (!sandbox) return NULL;
	return sandbox->module;
}

static inline uint64_t
sandbox_get_priority(void *element)
{
	struct sandbox *sandbox = (struct sandbox *)element;
	if (scheduler == SCHEDULER_SJF) return sandbox->remaining_exec;
	return sandbox->absolute_deadline;
}

static inline uint64_t
sandbox_get_priority_global(void *element)
{
	struct sandbox *sandbox = (struct sandbox *)element;
	return sandbox->absolute_deadline - sandbox->remaining_exec;
}

static inline void
sandbox_update_pq_idx_in_runqueue(void *element, size_t idx)
{
	assert(element);
	struct sandbox *sandbox     = (struct sandbox *)element;
	sandbox->pq_idx_in_runqueue = idx;
}

static inline void
local_sandbox_meta_update_pq_idx_in_tenant_queue(void *element, size_t idx)
{
	assert(element);
	struct sandbox_metadata *sandbox_meta = (struct sandbox_metadata *)element;
	sandbox_meta->pq_idx_in_tenant_queue  = idx;
}
