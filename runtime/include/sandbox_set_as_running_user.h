#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "current_sandbox.h"
#include "panic.h"
#include "sandbox_types.h"
#include "sandbox_functions.h"

static inline void
sandbox_set_as_running_user(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->timestamp_of.last_state_change;

	sandbox->state = SANDBOX_SET_AS_RUNNING_USER;

	switch (last_state) {
	case SANDBOX_RUNNING_KERNEL: {
		sandbox->duration_of_state.running_user += duration_of_last_state;
		assert(sandbox == current_sandbox_get());
		assert(runtime_worker_threads_deadline[worker_thread_idx] == sandbox->absolute_deadline);

		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Running\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->timestamp_of.last_state_change = now;
	sandbox->state                          = SANDBOX_RUNNING_USER;

	/* State Change Bookkeeping */
	sandbox_state_history_append(sandbox, SANDBOX_RUNNING_USER);
	runtime_sandbox_total_increment(SANDBOX_RUNNING_USER);
	runtime_sandbox_total_decrement(last_state);

	/* Enable preemption at the end of state transition*/
	assert(last_state == SANDBOX_RUNNING_KERNEL);

	sandbox_enable_preemption(sandbox);
}
