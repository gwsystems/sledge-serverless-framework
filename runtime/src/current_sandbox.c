#include "current_sandbox.h"
#include "local_runqueue.h"
#include "worker_thread.h"

/* current sandbox that is active.. */
static __thread struct sandbox *worker_thread_current_sandbox = NULL;

__thread struct sandbox_context_cache local_sandbox_context_cache = {
	.linear_memory_start   = NULL,
	.linear_memory_size    = 0,
	.module_indirect_table = NULL,
};

/**
 * Getter for the current sandbox executing on this thread
 * @returns the current sandbox executing on this thread
 */
struct sandbox *
current_sandbox_get(void)
{
	return worker_thread_current_sandbox;
}

/**
 * Setter for the current sandbox executing on this thread
 * @param sandbox the sandbox we are setting this thread to run
 */
void
current_sandbox_set(struct sandbox *sandbox)
{
	/* Unpack hierarchy to avoid pointer chasing */
	if (sandbox == NULL) {
		local_sandbox_context_cache = (struct sandbox_context_cache){
			.linear_memory_start   = NULL,
			.linear_memory_size    = 0,
			.module_indirect_table = NULL,
		};
		worker_thread_current_sandbox = NULL;
	} else {
		local_sandbox_context_cache = (struct sandbox_context_cache){
			.linear_memory_start   = sandbox->linear_memory_start,
			.linear_memory_size    = sandbox->linear_memory_size,
			.module_indirect_table = sandbox->module->indirect_table,
		};
		worker_thread_current_sandbox = sandbox;
	}
}

/**
 * Mark the currently executing sandbox as blocked, remove it from the local runqueue,
 * and switch to base context
 */
void
current_sandbox_block(void)
{
	software_interrupt_disable();

	/* Remove the sandbox we were just executing from the runqueue and mark as blocked */
	struct sandbox *current_sandbox = current_sandbox_get();

	assert(current_sandbox->state == SANDBOX_RUNNING);
	sandbox_set_as_blocked(current_sandbox, SANDBOX_RUNNING);
	generic_thread_dump_lock_overhead();

	/* The worker thread seems to "spin" on a blocked sandbox, so try to execute another sandbox for one quantum
	 * after blocking to give time for the action to resolve */
	struct sandbox *next_sandbox = local_runqueue_get_next();
	if (next_sandbox != NULL) {
		sandbox_switch_to(next_sandbox);
	} else {
		worker_thread_switch_to_base_context();
	};
}
