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
	sandbox->state = SANDBOX_BLOCKED;
	uint64_t now   = __getcycles();

	switch (last_state) {
	case SANDBOX_RUNNING_KERNEL: {
		local_runqueue_delete(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Blocked\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	/* State Change Bookkeeping */
	sandbox->duration_of_state[last_state] += (now - sandbox->timestamp_of.last_state_change);
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_append(sandbox, SANDBOX_BLOCKED);
	runtime_sandbox_total_increment(SANDBOX_BLOCKED);
	runtime_sandbox_total_decrement(last_state);
}
