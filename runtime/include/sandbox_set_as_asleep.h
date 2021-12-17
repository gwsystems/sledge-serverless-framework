#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "local_runqueue.h"
#include "sandbox_types.h"
#include "sandbox_state.h"
#include "sandbox_state_history.h"

/**
 * Transitions a sandbox to the SANDBOX_ASLEEP state.
 * This occurs when a sandbox is executing and it makes a blocking API call of some kind.
 * Automatically removes the sandbox from the runqueue
 * @param sandbox the blocking sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_asleep(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	sandbox->state = SANDBOX_ASLEEP;
	uint64_t now   = __getcycles();

	switch (last_state) {
	case SANDBOX_RUNNING_SYS: {
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
	sandbox_state_history_append(&sandbox->state_history, SANDBOX_ASLEEP);
	sandbox_state_totals_increment(SANDBOX_ASLEEP);
	sandbox_state_totals_decrement(last_state);
}

static inline void
sandbox_sleep(struct sandbox *sandbox)
{
	assert(sandbox->state == SANDBOX_RUNNING_SYS);
	sandbox_set_as_asleep(sandbox, SANDBOX_RUNNING_SYS);
}
