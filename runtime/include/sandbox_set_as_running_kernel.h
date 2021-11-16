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
sandbox_set_as_running_kernel(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	sandbox->state = SANDBOX_RUNNING_KERNEL;
	uint64_t now   = __getcycles();

	switch (last_state) {
	case SANDBOX_RUNNING_USER: {
		assert(sandbox == current_sandbox_get());
		assert(runtime_worker_threads_deadline[worker_thread_idx] == sandbox->absolute_deadline);
		break;
	}
	case SANDBOX_RUNNABLE: {
		assert(sandbox);
		current_sandbox_set(sandbox);
		/* Does not handle context switch because the caller knows if we need to use fast or slow switched. We
		 * can fix this by breakout out SANDBOX_RUNNABLE and SANDBOX_PREEMPTED */
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Running Kernel\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	/* State Change Bookkeeping */
	sandbox->duration_of_state[last_state] += (now - sandbox->timestamp_of.last_state_change);
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_append(sandbox, SANDBOX_RUNNING_KERNEL);
	runtime_sandbox_total_increment(SANDBOX_RUNNING_KERNEL);
	runtime_sandbox_total_decrement(last_state);
}
