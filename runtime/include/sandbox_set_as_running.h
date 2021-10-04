#pragma once

#include <assert.h>
#include <stdint.h>

#include "memlogging.h"
#include "arch/getcycles.h"
#include "panic.h"
#include "sandbox_types.h"

extern uint64_t system_start_timestamp;
static inline void
sandbox_set_as_running(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_RUNNING;

	switch (last_state) {
	case SANDBOX_RUNNABLE: {
		uint64_t start_execution = now - system_start_timestamp;
		uint64_t last = sandbox->last_update_timestamp;
		uint32_t last_rs = sandbox->remaining_slack;
		sandbox->remaining_slack -= (now - sandbox->last_update_timestamp);
		sandbox->last_update_timestamp = now;  
		sandbox->runnable_duration += duration_of_last_state;
		current_sandbox_set(sandbox);
		runtime_worker_threads_deadline[worker_thread_idx] = sandbox->absolute_deadline;
                mem_log("time %lu sandbox starts running, request id:%d name %s obj=%p remaining slack %lu, last_rs %u now %lu last %lu \n", start_execution,
                         sandbox->id, sandbox->module->name, sandbox, sandbox->remaining_slack, last_rs, now, last);
		/* Does not handle context switch because the caller knows if we need to use fast or slow switched */
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Running\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RUNNING;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_RUNNING);
	runtime_sandbox_total_increment(SANDBOX_RUNNING);
	runtime_sandbox_total_decrement(last_state);
}
