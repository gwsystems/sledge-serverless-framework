#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "panic.h"
#include "sandbox_functions.h"
#include "sandbox_perf_log.h"
#include "sandbox_state.h"
#include "sandbox_state_history.h"
#include "sandbox_state_transition.h"
#include "sandbox_summarize_page_allocations.h"
#include "sandbox_types.h"
#include "execution_histogram.h"
#include "admissions_control.h"

/**
 * Transitions a sandbox from the SANDBOX_RETURNED state to the SANDBOX_COMPLETE state.
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
	assert(now > sandbox->timestamp_of.last_state_change);
	sandbox->last_state_duration = now - sandbox->timestamp_of.last_state_change;
	sandbox->duration_of_state[last_state] += sandbox->last_state_duration;
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_append(&sandbox->state_history, SANDBOX_COMPLETE);
	sandbox_state_totals_increment(SANDBOX_COMPLETE);
	sandbox_state_totals_decrement(last_state);

	struct route *route = sandbox->route;

#ifdef EXECUTION_HISTOGRAM
	/* Execution Histogram Post Processing */
	const uint64_t execution_duration = sandbox->duration_of_state[SANDBOX_RUNNING_USER]
	                                    + sandbox->duration_of_state[SANDBOX_RUNNING_SYS];
	execution_histogram_update(&route->execution_histogram, execution_duration);
#endif

#ifdef ADMISSIONS_CONTROL
	/* Admissions Control Post Processing */
	admissions_control_subtract(sandbox->admissions_estimate);
#endif

	/* Terminal State Logging for Sandbox */
	sandbox_perf_log_print_entry(sandbox);
	sandbox_summarize_page_allocations(sandbox);
	route_latency_add(&route->latency, sandbox->total_time);

	/* State Change Hooks */
	sandbox_state_transition_from_hook(sandbox, last_state);
	sandbox_state_transition_to_hook(sandbox, SANDBOX_COMPLETE);

	/* Does not add to cleanup queue until in cooperative scheduler */
}

static inline void
sandbox_exit_success(struct sandbox *sandbox)
{
	assert(sandbox->state == SANDBOX_RETURNED);
	sandbox_set_as_complete(sandbox, SANDBOX_RETURNED);
}
