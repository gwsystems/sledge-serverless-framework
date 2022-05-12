#pragma once

#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>

#include "panic.h"
#include "tenant.h"
#include "sandbox_types.h"

/***************************
 * Public API              *
 **************************/

struct sandbox *sandbox_alloc(struct module *module, struct http_session *session, struct route *route,
                              struct tenant *tenant, uint64_t admissions_estimate);
int             sandbox_prepare_execution_environment(struct sandbox *sandbox);
void            sandbox_free(struct sandbox *sandbox);
void            sandbox_main(struct sandbox *sandbox);
void            sandbox_switch_to(struct sandbox *next_sandbox);

/**
 * Free Linear Memory, leaving stack in place
 * @param sandbox
 */
static inline void
sandbox_free_linear_memory(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox->memory != NULL);
	module_free_linear_memory(sandbox->module, (struct wasm_memory *)sandbox->memory);
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
	return sandbox->absolute_deadline;
};
