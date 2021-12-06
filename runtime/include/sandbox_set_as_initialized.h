#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "arch/context.h"
#include "current_sandbox.h"
#include "ps_list.h"
#include "sandbox_state_history.h"
#include "sandbox_types.h"

/**
 * Transitions a sandbox to the SANDBOX_INITIALIZED state.
 * The sandbox was already zeroed out during allocation
 * @param sandbox
 * @param last_state
 */
static inline void
sandbox_set_as_initialized(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	sandbox->state = SANDBOX_INITIALIZED;
	uint64_t now   = __getcycles();

	switch (last_state) {
	case SANDBOX_ALLOCATED: {
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Preempted\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	/* State Change Bookkeeping */
	sandbox->duration_of_state[last_state] += (now - sandbox->timestamp_of.last_state_change);
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_append(&sandbox->state_history, SANDBOX_INITIALIZED);
	sandbox_state_totals_increment(SANDBOX_INITIALIZED);
	sandbox_state_totals_decrement(last_state);
}
