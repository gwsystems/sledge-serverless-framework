#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "current_sandbox.h"
#include "panic.h"
#include "sandbox_state_history.h"
#include "sandbox_types.h"
#include "sandbox_functions.h"

static inline void
sandbox_set_as_running_user(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);

	uint64_t now = __getcycles();

	switch (last_state) {
	case SANDBOX_RUNNING_KERNEL: {
		assert(sandbox == current_sandbox_get());
		assert(runtime_worker_threads_deadline[worker_thread_idx] == sandbox->absolute_deadline);
		break;
	}
	case SANDBOX_PREEMPTED: {
		assert(sandbox);
		current_sandbox_set(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Running\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}


	/* State Change Bookkeeping */
	sandbox->duration_of_state[last_state] += (now - sandbox->timestamp_of.last_state_change);
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_append(sandbox, SANDBOX_RUNNING_USER);
	runtime_sandbox_total_increment(SANDBOX_RUNNING_USER);
	runtime_sandbox_total_decrement(last_state);

	/* WARNING: This state change needs to be at the end of this transition because all code below this assignment
	 * is preemptable */
	sandbox->state = SANDBOX_RUNNING_USER;
}
