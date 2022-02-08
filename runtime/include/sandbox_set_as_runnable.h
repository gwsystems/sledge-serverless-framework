#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "local_runqueue.h"
#include "panic.h"
#include "sandbox_state_history.h"
#include "sandbox_types.h"

/**
 * Transitions a sandbox to the SANDBOX_RUNNABLE state.
 *
 * This occurs in the following scenarios:
 * - A sandbox in the SANDBOX_INITIALIZED state completes initialization and is ready to be run
 * - A sandbox in the SANDBOX_ASLEEP state completes what was blocking it and is ready to be run
 *
 * @param sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_runnable(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	sandbox->state = SANDBOX_RUNNABLE;
	uint64_t now   = __getcycles();

	switch (last_state) {
	case SANDBOX_INITIALIZED: {
		local_runqueue_add(sandbox);
		break;
	}
	case SANDBOX_ASLEEP: {
		local_runqueue_add(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Runnable\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	/* State Change Bookkeeping */
	assert(now > sandbox->timestamp_of.last_state_change);
	sandbox->duration_of_state[last_state] += (now - sandbox->timestamp_of.last_state_change);
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_append(&sandbox->state_history, SANDBOX_RUNNABLE);
	sandbox_state_totals_increment(SANDBOX_RUNNABLE);
	sandbox_state_totals_decrement(last_state);
}


static inline void
sandbox_wakeup(struct sandbox *sandbox)
{
	assert(sandbox->state == SANDBOX_ASLEEP);
	sandbox_set_as_runnable(sandbox, SANDBOX_ASLEEP);
}
