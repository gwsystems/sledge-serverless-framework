#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "debuglog.h"
#include "sandbox_state.h"

const char *sandbox_state_labels[SANDBOX_STATE_COUNT] = {

	[SANDBOX_UNINITIALIZED] = "Uninitialized",
	[SANDBOX_ALLOCATED]     = "Allocated",
	[SANDBOX_INITIALIZED]   = "Initialized",
	[SANDBOX_RUNNABLE]      = "Runnable",
	[SANDBOX_INTERRUPTED]   = "Interrupted",
	[SANDBOX_PREEMPTED]     = "Preempted",
	[SANDBOX_RUNNING_SYS]   = "Running Sys",
	[SANDBOX_RUNNING_USER]  = "Running User",
	[SANDBOX_ASLEEP]        = "Asleep",
	[SANDBOX_RETURNED]      = "Returned",
	[SANDBOX_COMPLETE]      = "Complete",
	[SANDBOX_ERROR]         = "Error"
};

#ifdef LOG_SANDBOX_COUNT
_Atomic uint32_t sandbox_state_count[SANDBOX_STATE_COUNT];
#endif

/*
 * Function intended to be interactively run in a debugger to look at sandbox totals
 * via `call runtime_log_sandbox_states()`
 */
void
runtime_log_sandbox_states()
{
#ifdef LOG_SANDBOX_COUNT
	const size_t buffer_size         = 1000;
	char         buffer[buffer_size] = "";
	for (int i = 0; i < SANDBOX_STATE_COUNT; i++) {
		const size_t tiny_buffer_size              = 50;
		char         tiny_buffer[tiny_buffer_size] = "";
		snprintf(tiny_buffer, tiny_buffer_size - 1, "%s: %u\n\t", sandbox_state_stringify(i),
		         atomic_load(&sandbox_state_count[i]));
		strncat(buffer, tiny_buffer, buffer_size - 1 - strlen(buffer));
	}

	debuglog("%s", buffer);

#else
	debuglog("Must compile with LOG_SANDBOX_COUNT for this functionality!\n");
#endif
};
