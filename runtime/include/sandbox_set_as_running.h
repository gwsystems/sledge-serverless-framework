#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "panic.h"
#include "sandbox_types.h"

static inline void
sandbox_set_as_running(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->timestamp_of.last_state_change;

	sandbox->state = SANDBOX_SET_AS_RUNNING;

	switch (last_state) {
	case SANDBOX_RUNNABLE: {
		sandbox->runnable_duration += duration_of_last_state;
		current_sandbox_set(sandbox);
		runtime_worker_threads_deadline[worker_thread_idx] = sandbox->absolute_deadline;
		/* Does not handle context switch because the caller knows if we need to use fast or slow switched */
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Running\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->timestamp_of.last_state_change = now;
	sandbox->state                          = SANDBOX_RUNNING;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_RUNNING);
	runtime_sandbox_total_increment(SANDBOX_RUNNING);
	runtime_sandbox_total_decrement(last_state);
}
