#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "arch/context.h"
#include "current_sandbox.h"
#include "sandbox_state_history.h"
#include "sandbox_state_transition.h"
#include "sandbox_types.h"

/**
 * Transitions a sandbox to the SANDBOX_ALLOCATED state.
 * This the is the initial state, so there is no concept of "last state" here
 * @param sandbox
 */
static inline void
sandbox_set_as_allocated(struct sandbox *sandbox)
{
	assert(sandbox);
	assert(sandbox->state == SANDBOX_UNINITIALIZED);
	sandbox->state = SANDBOX_ALLOCATED;
	uint64_t now = __getcycles();

	/* State Change Bookkeeping */
	assert(now > sandbox->timestamp_of.last_state_change);
	// sandbox->timestamp_of.allocation        = now;
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_init(&sandbox->state_history);
	sandbox_state_history_append(&sandbox->state_history, SANDBOX_ALLOCATED);
	sandbox_state_totals_increment(SANDBOX_ALLOCATED);

	/* State Change Hooks */
	sandbox_state_transition_from_hook(sandbox, SANDBOX_UNINITIALIZED);
	sandbox_state_transition_to_hook(sandbox, SANDBOX_ALLOCATED);
}
