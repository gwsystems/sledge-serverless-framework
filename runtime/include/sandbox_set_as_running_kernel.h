#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "current_sandbox.h"
#include "panic.h"
#include "sandbox_types.h"
#include "sandbox_functions.h"

static inline void
sandbox_set_as_running_kernel(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);

	/* Disable preemption at start of state transition */
	if (last_state == SANDBOX_RUNNING_USER) {
		assert(sandbox->ctxt.preemptable == true);
		sandbox_disable_preemption(sandbox);
	} else {
		assert(sandbox->ctxt.preemptable == false);
	}

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->timestamp_of.last_state_change;

	sandbox->state = SANDBOX_SET_AS_RUNNING_KERNEL;
	sandbox_state_history_append(sandbox, SANDBOX_SET_AS_RUNNING_KERNEL);

	switch (last_state) {
	case SANDBOX_RUNNING_USER: {
		assert(sandbox == current_sandbox_get());
		sandbox->duration_of_state.running_user += duration_of_last_state;
		assert(runtime_worker_threads_deadline[worker_thread_idx] == sandbox->absolute_deadline);
		break;
	}
	case SANDBOX_RUNNABLE: {
		assert(sandbox);
		sandbox->duration_of_state.runnable += duration_of_last_state;
		current_sandbox_set(sandbox);
		/* Does not handle context switch because the caller knows if we need to use fast or slow switched. We
		 * can fix this by breakout out SANDBOX_RUNNABLE and SANDBOX_PREEMPTED */
		break;
	}
	case SANDBOX_PREEMPTED: {
		assert(sandbox);
		assert(sandbox->interrupted_state == SANDBOX_RUNNING_USER);
		sandbox->duration_of_state.preempted += duration_of_last_state;
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

	sandbox->timestamp_of.last_state_change = now;
	sandbox->state                          = SANDBOX_RUNNING_KERNEL;

	/* State Change Bookkeeping */
	sandbox_state_history_append(sandbox, SANDBOX_RUNNING_KERNEL);
	runtime_sandbox_total_increment(SANDBOX_RUNNING_KERNEL);
	runtime_sandbox_total_decrement(last_state);
}
