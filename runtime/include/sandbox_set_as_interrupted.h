#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "current_sandbox.h"
#include "panic.h"
#include "sandbox_functions.h"
#include "sandbox_state_history.h"
#include "sandbox_state_transition.h"
#include "sandbox_types.h"

static inline void
sandbox_set_as_interrupted(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);

	/* WARNING: All code before this assignment is preemptable */
	sandbox->state = SANDBOX_INTERRUPTED;
	barrier();

	uint64_t now = __getcycles();

	/* State Change Bookkeeping */
	assert(now > sandbox->timestamp_of.last_state_change);
	sandbox->last_state_duration = now - sandbox->timestamp_of.last_state_change;
	assert(last_state == SANDBOX_RUNNING_USER);
	sandbox->remaining_exec = (sandbox->remaining_exec > sandbox->last_state_duration)
	                            ? sandbox->remaining_exec - sandbox->last_state_duration
	                            : 0;
	sandbox->duration_of_state[last_state] += sandbox->last_state_duration;
	sandbox->timestamp_of.last_state_change = now;
	/* We do not append SANDBOX_INTERRUPTED to the sandbox_state_history because it would quickly fill the buffer */
	sandbox_state_totals_increment(SANDBOX_INTERRUPTED);
	sandbox_state_totals_decrement(last_state);

	/* State Change Hooks */
	sandbox_state_transition_from_hook(sandbox, last_state);
	sandbox_state_transition_to_hook(sandbox, SANDBOX_INTERRUPTED);
}

static inline void
sandbox_interrupt(struct sandbox *sandbox)
{
	sandbox_set_as_interrupted(sandbox, sandbox->state);

	sandbox_process_scheduler_updates(sandbox);
}


/**
 * @brief Transition sandbox back to interrupted state
 * @param sandbox
 * @param interrupted_state - state to return to
 */
static inline void
sandbox_interrupt_return(struct sandbox *sandbox, sandbox_state_t interrupted_state)
{
	assert(sandbox);
	assert(interrupted_state != SANDBOX_INTERRUPTED);

	uint64_t now = __getcycles();

	/* State Change Bookkeeping */
	sandbox->duration_of_state[SANDBOX_INTERRUPTED] += (now - sandbox->timestamp_of.last_state_change);
	sandbox->timestamp_of.last_state_change = now;
	/* We do not append SANDBOX_INTERRUPTED to the sandbox_state_history because it would quickly fill the buffer */
	sandbox_state_totals_increment(interrupted_state);
	sandbox_state_totals_decrement(SANDBOX_INTERRUPTED);

	if (sandbox->absolute_deadline < now) {
		// printf("Interrupted Sandbox missed deadline already!\n");
	}

	barrier();
	/* WARNING: Code after this assignment may be preemptable */
	sandbox->state = interrupted_state;
}
