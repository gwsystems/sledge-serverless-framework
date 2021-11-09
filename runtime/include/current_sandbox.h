#pragma once

#include <threads.h>

#include "sandbox_types.h"

/* current sandbox that is active.. */
extern thread_local struct sandbox *worker_thread_current_sandbox;

extern thread_local struct sandbox_context_cache local_sandbox_context_cache;

void current_sandbox_start(void);

/**
 * Getter for the current sandbox executing on this thread
 * @returns the current sandbox executing on this thread
 */
static inline struct sandbox *
current_sandbox_get(void)
{
	return worker_thread_current_sandbox;
}

/**
 * Setter for the current sandbox executing on this thread
 * @param sandbox the sandbox we are setting this thread to run
 */
static inline void
current_sandbox_set(struct sandbox *sandbox)
{
	/* Unpack hierarchy to avoid pointer chasing */
	if (sandbox == NULL) {
		local_sandbox_context_cache = (struct sandbox_context_cache){
			.memory = {
				.start          = NULL,
				.size           = 0,
				.max           = 0,
			},
			.module_indirect_table = NULL,
		};
		worker_thread_current_sandbox                      = NULL;
		runtime_worker_threads_deadline[worker_thread_idx] = UINT64_MAX;
	} else {
		local_sandbox_context_cache = (struct sandbox_context_cache){
			.memory                = sandbox->memory,
			.module_indirect_table = sandbox->module->indirect_table,
		};
		worker_thread_current_sandbox                      = sandbox;
		runtime_worker_threads_deadline[worker_thread_idx] = sandbox->absolute_deadline;
	}
}
