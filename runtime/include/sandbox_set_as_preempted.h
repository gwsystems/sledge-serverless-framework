#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "local_runqueue.h"
#include "panic.h"
#include "sandbox_state_history.h"
#include "sandbox_state_transition.h"
#include "sandbox_types.h"

/**
 * Transitions a sandbox to the SANDBOX_PREEMPTED state.
 *
 * This occurs when a sandbox is preempted in the SIGALRM handler
 *
 * @param sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_preempted(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	sandbox->state = SANDBOX_PREEMPTED;
	uint64_t now   = __getcycles();

	switch (last_state) {
	case SANDBOX_INTERRUPTED:
		break;
	default: 
		panic("Sandbox %lu | Illegal transition from %s to Preempted\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}

	/* State Change Bookkeeping */
	assert(now > sandbox->timestamp_of.last_state_change);
	sandbox->last_state_duration = now - sandbox->timestamp_of.last_state_change;
	sandbox->duration_of_state[last_state] += sandbox->last_state_duration;
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_append(&sandbox->state_history, SANDBOX_PREEMPTED);
	sandbox_state_totals_increment(SANDBOX_PREEMPTED);
	sandbox_state_totals_decrement(last_state);

	/* State Change Hooks */
	sandbox_state_transition_from_hook(sandbox, last_state);
	sandbox_state_transition_to_hook(sandbox, SANDBOX_PREEMPTED);
}

static inline void
sandbox_preempt(struct sandbox *sandbox)
{
	assert(sandbox->state == SANDBOX_INTERRUPTED);
	sandbox_set_as_preempted(sandbox, SANDBOX_INTERRUPTED);

	// sandbox_process_scheduler_updates(sandbox);
}
