#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "local_runqueue.h"
#include "panic.h"
#include "sandbox_functions.h"
#include "sandbox_state.h"
#include "sandbox_state_history.h"
#include "sandbox_types.h"

/**
 * Transitions a sandbox to the SANDBOX_RETURNED state.
 * This occurs when a sandbox is executing and runs to completion.
 * Automatically removes the sandbox from the runqueue and frees linear memory.
 * Because the stack is still in use, freeing the stack is deferred until later
 * @param sandbox the blocking sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_returned(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	sandbox->state = SANDBOX_RETURNED;
	uint64_t now   = __getcycles();

	switch (last_state) {
	case SANDBOX_RUNNING_SYS: {
		sandbox->timestamp_of.response = now;
		sandbox->total_time            = now - sandbox->timestamp_of.request_arrival;
		local_runqueue_delete(sandbox);
		sandbox_free_linear_memory(sandbox);
		sandbox_deinit_http_buffers(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Returned\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	/* State Change Bookkeeping */
	sandbox->duration_of_state[last_state] += (now - sandbox->timestamp_of.last_state_change);
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_append(&sandbox->state_history, SANDBOX_RETURNED);
	sandbox_state_totals_increment(SANDBOX_RETURNED);
	sandbox_state_totals_decrement(last_state);
}
