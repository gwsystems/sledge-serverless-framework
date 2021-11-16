#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "panic.h"
#include "local_completion_queue.h"
#include "sandbox_functions.h"
#include "sandbox_print_perf.h"
#include "sandbox_state.h"
#include "sandbox_state_history.h"
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
	sandbox->state = SANDBOX_COMPLETE;
	uint64_t now   = __getcycles();

	switch (last_state) {
	case SANDBOX_RETURNED: {
		sandbox->timestamp_of.completion = now;
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Error\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	/* State Change Bookkeeping */
	sandbox->duration_of_state[last_state] += (now - sandbox->timestamp_of.last_state_change);
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_append(sandbox, SANDBOX_COMPLETE);
	runtime_sandbox_total_increment(SANDBOX_COMPLETE);
	runtime_sandbox_total_decrement(last_state);

	/* Admissions Control Post Processing */
	admissions_info_update(&sandbox->module->admissions_info,
	                       sandbox->duration_of_state[SANDBOX_RUNNING_USER]
	                         + sandbox->duration_of_state[SANDBOX_RUNNING_KERNEL]);
	admissions_control_subtract(sandbox->admissions_estimate);

	/* Terminal State Logging */
	sandbox_print_perf(sandbox);
	sandbox_summarize_page_allocations(sandbox);

	/* Do not touch sandbox state after adding to completion queue to avoid use-after-free bugs */
	local_completion_queue_add(sandbox);
}
