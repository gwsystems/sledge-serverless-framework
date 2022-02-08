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
sandbox_set_as_running_sys(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);

	/* WARNING: All code before this assignment is preemptable if transitioning from RUNNING_USER */
	sandbox->state = SANDBOX_RUNNING_SYS;
	barrier();

	uint64_t now = __getcycles();

	switch (last_state) {
	case SANDBOX_RUNNING_USER: {
		assert(sandbox == current_sandbox_get());
		assert(runtime_worker_threads_deadline[worker_thread_idx] == sandbox->absolute_deadline);
		break;
	}
	case SANDBOX_RUNNABLE: {
		assert(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Running Sys\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	/* State Change Bookkeeping */
	assert(now > sandbox->timestamp_of.last_state_change);
	sandbox->duration_of_state[last_state] += (now - sandbox->timestamp_of.last_state_change);
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_append(&sandbox->state_history, SANDBOX_RUNNING_SYS);
	sandbox_state_totals_increment(SANDBOX_RUNNING_SYS);
	sandbox_state_totals_decrement(last_state);
}

static inline void
sandbox_syscall(struct sandbox *sandbox)
{
	assert(sandbox->state == SANDBOX_RUNNING_USER);
	sandbox_set_as_running_sys(sandbox, SANDBOX_RUNNING_USER);
}
