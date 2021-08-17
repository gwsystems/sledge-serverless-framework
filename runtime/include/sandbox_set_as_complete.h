#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "panic.h"
#include "local_completion_queue.h"
#include "sandbox_functions.h"
#include "sandbox_state.h"
#include "sandbox_summarize_page_allocations.h"
#include "sandbox_types.h"

/**
 * Transitions a sandbox from the SANDBOX_RETURNED state to the SANDBOX_COMPLETE state.
 * Adds the sandbox to the completion queue
 * @param sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_complete(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->timestamp_of.last_state_change;

	sandbox->state = SANDBOX_SET_AS_COMPLETE;

	switch (last_state) {
	case SANDBOX_RETURNED: {
		sandbox->timestamp_of.completion = now;
		sandbox->duration_of_state.returned += duration_of_last_state;
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Error\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->timestamp_of.last_state_change = now;
	sandbox->state                          = SANDBOX_COMPLETE;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_COMPLETE);
	runtime_sandbox_total_increment(SANDBOX_COMPLETE);
	runtime_sandbox_total_decrement(last_state);

	/* Admissions Control Post Processing */
	admissions_info_update(&sandbox->module->admissions_info, sandbox->duration_of_state.running);
	admissions_control_subtract(sandbox->admissions_estimate);

	/* Terminal State Logging */
	sandbox_print_perf(sandbox);
	sandbox_summarize_page_allocations(sandbox);

	/* Do not touch sandbox state after adding to completion queue to avoid use-after-free bugs */
	local_completion_queue_add(sandbox);
}
