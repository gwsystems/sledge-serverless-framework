#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "panic.h"
#include "sandbox_state.h"
#include "sandbox_types.h"

/**
 * Transitions a sandbox to the SANDBOX_PREEMPTED state.
 *
 * This occurs when a sandbox is executing and in a RUNNING state and a SIGALRM software interrupt fires
 * and pulls a sandbox with an earlier absolute deadline from the global request scheduler.
 *
 * @param sandbox the sandbox being preempted
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_preempted(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_PREEMPTED;

	switch (last_state) {
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Preempted\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_PREEMPTED;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_PREEMPTED);
	runtime_sandbox_total_increment(SANDBOX_PREEMPTED);
	runtime_sandbox_total_decrement(SANDBOX_RUNNING);
}
