#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "local_runqueue.h"
#include "panic.h"
#include "sandbox_functions.h"
#include "sandbox_state.h"
#include "sandbox_types.h"
#include "software_interrupt.h"

/**
 * Transitions a sandbox to the SANDBOX_RETURNED state.
 * This occurs when a sandbox is executing and runs to completion.
 * Automatically removes the sandbox from the runqueue and unmaps linear memory.
 * Because the stack is still in use, freeing the stack is deferred until later
 * @param sandbox the blocking sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_returned(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_RETURNED;

	switch (last_state) {
	case SANDBOX_RUNNING: {
		sandbox->response_timestamp = now;
		sandbox->total_time         = now - sandbox->request_arrival_timestamp;
		sandbox->running_duration += duration_of_last_state;
		local_runqueue_delete(sandbox);
		sandbox_free_linear_memory(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Returned\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RETURNED;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_RETURNED);
	runtime_sandbox_total_increment(SANDBOX_RETURNED);
	runtime_sandbox_total_decrement(last_state);
}
