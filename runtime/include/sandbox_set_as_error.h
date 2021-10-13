#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "local_completion_queue.h"
#include "local_runqueue.h"
#include "sandbox_state.h"
#include "sandbox_functions.h"
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

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->timestamp_of.last_state_change;

	sandbox->state = SANDBOX_SET_AS_ERROR;

	switch (last_state) {
	case SANDBOX_SET_AS_INITIALIZED:
		/* Technically, this is a degenerate sandbox that we generate by hand */
		sandbox->duration_of_state.initializing += duration_of_last_state;
		break;
	case SANDBOX_RUNNING: {
		sandbox->duration_of_state.running += duration_of_last_state;
		local_runqueue_delete(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Error\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	uint64_t sandbox_id = sandbox->id;
	sandbox->state      = SANDBOX_ERROR;
	sandbox_print_perf(sandbox);
	sandbox_summarize_page_allocations(sandbox);
	sandbox_free_linear_memory(sandbox);
	admissions_control_subtract(sandbox->admissions_estimate);
	/* Do not touch sandbox after adding to completion queue to avoid use-after-free bugs */
	local_completion_queue_add(sandbox);

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox_id, last_state, SANDBOX_ERROR);
	runtime_sandbox_total_increment(SANDBOX_ERROR);
	runtime_sandbox_total_decrement(last_state);
}
