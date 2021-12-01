#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "current_sandbox.h"
#include "panic.h"
#include "sandbox_functions.h"
#include "sandbox_state_history.h"
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
	sandbox->duration_of_state[last_state] += (now - sandbox->timestamp_of.last_state_change);
	sandbox->timestamp_of.last_state_change = now;
	/* We do not append SANDBOX_INTERRUPTED to the sandbox_state_history because it would quickly fill the buffer */
	runtime_sandbox_total_increment(SANDBOX_INTERRUPTED);
	runtime_sandbox_total_decrement(last_state);
}

static inline void
sandbox_interrupt(struct sandbox *sandbox)
{
	sandbox_set_as_interrupted(sandbox, sandbox->state);
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
	runtime_sandbox_total_increment(interrupted_state);
	runtime_sandbox_total_decrement(SANDBOX_INTERRUPTED);

	barrier();
	/* WARNING: Code after this assignment may be preemptable */
	sandbox->state = interrupted_state;
}
