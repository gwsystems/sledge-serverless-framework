#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "panic.h"
#include "software_interrupt.h"
#include "sandbox_types.h"

/**
 * Transitions a sandbox to the SANDBOX_RUNNABLE state.
 *
 * This occurs in the following scenarios:
 * - A sandbox in the SANDBOX_INITIALIZED state completes initialization and is ready to be run
 * - A sandbox in the SANDBOX_BLOCKED state completes what was blocking it and is ready to be run
 *
 * @param sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_runnable(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_RUNNABLE;

	switch (last_state) {
	case SANDBOX_INITIALIZED: {
		sandbox->initializing_duration += duration_of_last_state;
		break;
	}
	case SANDBOX_BLOCKED: {
		sandbox->blocked_duration += duration_of_last_state;
		break;
	}
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Runnable\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RUNNABLE;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_RUNNABLE);
	runtime_sandbox_total_increment(SANDBOX_RUNNABLE);
	runtime_sandbox_total_decrement(last_state);
}
