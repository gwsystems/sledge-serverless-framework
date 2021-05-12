#pragma once

#include <assert.h>
#include <stddef.h>

#include "current_sandbox.h"
#include "current_sandbox_yield.h"
#include "generic_thread.h"
#include "local_runqueue.h"
#include "sandbox_set_as_blocked.h"
#include "software_interrupt.h"

/**
 * Mark the currently executing sandbox as blocked, remove it from the local runqueue,
 * and switch to base context
 */
static inline void
current_sandbox_block(void)
{
	/* Remove the sandbox we were just executing from the runqueue and mark as blocked */
	struct sandbox *current_sandbox = current_sandbox_get();

	/* We might either have blocked in start reading the request or while executing within the WebAssembly
	 * entrypoint. The preemptable flag on the context is used to differentiate. In either case, we should
	 * have disabled interrupts.
	 */
	if (current_sandbox->ctxt.preemptable) software_interrupt_disable();
	assert(!software_interrupt_is_enabled());

	assert(current_sandbox->state == SANDBOX_RUNNING);
	sandbox_set_as_blocked(current_sandbox, SANDBOX_RUNNING);
	generic_thread_dump_lock_overhead();

	/* The worker thread seems to "spin" on a blocked sandbox, so try to execute another sandbox for one quantum
	 * after blocking to give time for the action to resolve */
	struct sandbox *next_sandbox = local_runqueue_get_next();
	if (next_sandbox != NULL) {
		sandbox_switch_to(next_sandbox);
	} else {
		current_sandbox_yield();
	};
}
