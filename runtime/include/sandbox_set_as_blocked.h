#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "local_runqueue.h"
#include "sandbox_types.h"
#include "sandbox_state.h"

/**
 * Transitions a sandbox to the SANDBOX_BLOCKED state.
 * This occurs when a sandbox is executing and it makes a blocking API call of some kind.
 * Automatically removes the sandbox from the runqueue
 * @param sandbox the blocking sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_blocked(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->timestamp_of.last_state_change;

	sandbox->state = SANDBOX_SET_AS_BLOCKED;

	switch (last_state) {
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		local_runqueue_delete(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Blocked\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->timestamp_of.last_state_change = now;
	sandbox->state                          = SANDBOX_BLOCKED;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_BLOCKED);
	runtime_sandbox_total_increment(SANDBOX_BLOCKED);
	runtime_sandbox_total_decrement(last_state);
}
