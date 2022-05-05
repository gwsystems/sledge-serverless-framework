#pragma once

#include <assert.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "current_sandbox.h"
#include "panic.h"
#include "sandbox_state_history.h"
#include "sandbox_state_transition.h"
#include "sandbox_types.h"
#include "sandbox_functions.h"

static inline void
sandbox_set_as_running_user(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);

	uint64_t now = __getcycles();

	switch (last_state) {
	case SANDBOX_RUNNING_SYS: {
		assert(sandbox == current_sandbox_get());
		assert(runtime_worker_threads_deadline[worker_thread_idx] == sandbox->absolute_deadline);
		break;
	}
	case SANDBOX_PREEMPTED: {
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Running\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}


	/* State Change Bookkeeping */
	assert(now > sandbox->timestamp_of.last_state_change);
	sandbox->last_state_duration = now - sandbox->timestamp_of.last_state_change;
	sandbox->duration_of_state[last_state] += sandbox->last_state_duration;
	sandbox->timestamp_of.last_state_change = now;
	sandbox_state_history_append(&sandbox->state_history, SANDBOX_RUNNING_USER);
	sandbox_state_totals_increment(SANDBOX_RUNNING_USER);
	sandbox_state_totals_decrement(last_state);

	/* State Change Hooks */
	sandbox_state_transition_from_hook(sandbox, last_state);
	sandbox_state_transition_to_hook(sandbox, SANDBOX_RUNNING_USER);

	barrier();
	sandbox->state = SANDBOX_RUNNING_USER;
	/* WARNING: All code after this assignment is preemptable */

	/* Now that we are preemptable, we can replay deferred sigalrms */
	software_interrupt_deferred_sigalrm_replay();
}

static inline void
sandbox_return(struct sandbox *sandbox)
{
	assert(sandbox->state == SANDBOX_RUNNING_SYS);
	sandbox_set_as_running_user(sandbox, SANDBOX_RUNNING_SYS);
}
