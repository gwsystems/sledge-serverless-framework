#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "local_completion_queue.h"
#include "local_runqueue.h"
#include "sandbox_state.h"
#include "sandbox_functions.h"
#include "sandbox_perf_log.h"
#include "sandbox_state_history.h"
#include "sandbox_summarize_page_allocations.h"
#include "panic.h"

/**
 * Transitions a sandbox to the SANDBOX_ERROR state.
 * This can occur during initialization or execution
 * Unmaps linear memory, removes from the runqueue (if on it), and adds to the completion queue
 * Because the stack is still in use, freeing the stack is deferred until later
 *
 * @param sandbox the sandbox erroring out
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_error(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	sandbox->state = SANDBOX_ERROR;
	uint64_t now   = __getcycles();

	switch (last_state) {
	case SANDBOX_ALLOCATED:
		break;
	case SANDBOX_RUNNING_SYS: {
		local_runqueue_delete(sandbox);
		sandbox_free_linear_memory(sandbox);
		sandbox_deinit_http_buffers(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Error\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	/* State Change Bookkeeping */
	assert(now > sandbox->timestamp_of.last_state_change);
	sandbox->last_duration_of_exec = now - sandbox->timestamp_of.last_state_change;
	sandbox->duration_of_state[last_state] += sandbox->last_duration_of_exec;
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_append(&sandbox->state_history, SANDBOX_ERROR);
	sandbox_state_totals_increment(SANDBOX_ERROR);
	sandbox_state_totals_decrement(last_state);

	/* Admissions Control Post Processing */
	admissions_control_subtract(sandbox->admissions_estimate);

	/* Terminal State Logging */
	sandbox_perf_log_print_entry(sandbox);
	sandbox_summarize_page_allocations(sandbox);

	/* Does not add to completion queue until in cooperative scheduler */
}

static inline void
sandbox_exit_error(struct sandbox *sandbox)
{
	assert(sandbox->state == SANDBOX_RUNNING_SYS);
	sandbox_set_as_error(sandbox, SANDBOX_RUNNING_SYS);

	if (module_is_paid(sandbox->module)) {
		atomic_fetch_sub(&sandbox->module->remaining_budget, sandbox->last_duration_of_exec);
	}
}
