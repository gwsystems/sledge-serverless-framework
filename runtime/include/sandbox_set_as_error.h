#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "local_completion_queue.h"
#include "local_runqueue.h"
#include "sandbox_state.h"
#include "sandbox_functions.h"
#include "sandbox_print_perf.h"
#include "sandbox_state_history.h"
#include "sandbox_summarize_page_allocations.h"
#include "panic.h"

/**
 * Transitions a sandbox to the SANDBOX_ERROR state.
 * This can occur during initialization or execution
 * Unmaps linear memory, removes from the runqueue (if on it), and adds to the completion queue
 * Because the stack is still in use, freeing the stack is deferred until later
 *
 * TODO: Is the sandbox adding itself to the completion queue here? Is this a problem? Issue #94
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
	case SANDBOX_UNINITIALIZED:
		/* Technically, this is a degenerate sandbox that we generate by hand */
		break;
	case SANDBOX_RUNNING_SYS: {
		local_runqueue_delete(sandbox);
		sandbox_free_linear_memory(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Error\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	/* State Change Bookkeeping */
	uint64_t duration_of_last_state = now - sandbox->timestamp_of.last_state_change;
	sandbox->duration_of_state[last_state] += duration_of_last_state;
	sandbox_state_history_append(sandbox, SANDBOX_ERROR);
	runtime_sandbox_total_increment(SANDBOX_ERROR);
	runtime_sandbox_total_decrement(last_state);

	/* Admissions Control Post Processing */
	admissions_control_subtract(sandbox->admissions_estimate);

	/* Terminal State Logging */
	sandbox_print_perf(sandbox);
	sandbox_summarize_page_allocations(sandbox);

	/* Do not touch sandbox after adding to completion queue to avoid use-after-free bugs */
	local_completion_queue_add(sandbox);
}

static inline void
sandbox_exit_error(struct sandbox *sandbox)
{
	assert(sandbox->state == SANDBOX_RUNNING_SYS);
	sandbox_set_as_error(sandbox, SANDBOX_RUNNING_SYS);
}
