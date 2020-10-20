#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "debuglog.h"
#include "sandbox_state.h"

const bool sandbox_state_is_terminal[SANDBOX_STATE_COUNT] = {

	[SANDBOX_UNINITIALIZED] = false, [SANDBOX_ALLOCATED] = false,
	[SANDBOX_INITIALIZED] = true,    [SANDBOX_SET_AS_RUNNABLE] = false,
	[SANDBOX_RUNNABLE] = true,       [SANDBOX_SET_AS_RUNNING] = false,
	[SANDBOX_RUNNING] = true,        [SANDBOX_SET_AS_PREEMPTED] = false,
	[SANDBOX_PREEMPTED] = true,      [SANDBOX_SET_AS_BLOCKED] = false,
	[SANDBOX_BLOCKED] = true,        [SANDBOX_SET_AS_RETURNED] = false,
	[SANDBOX_RETURNED] = true,       [SANDBOX_SET_AS_COMPLETE] = false,
	[SANDBOX_COMPLETE] = true,       [SANDBOX_SET_AS_ERROR] = false,
	[SANDBOX_ERROR] = true
};

const char *sandbox_state_labels[SANDBOX_STATE_COUNT] = {

	[SANDBOX_UNINITIALIZED]      = "Uninitialized",
	[SANDBOX_ALLOCATED]          = "Allocated",
	[SANDBOX_SET_AS_INITIALIZED] = "Transitioning to Initialized",
	[SANDBOX_INITIALIZED]        = "Initialized",
	[SANDBOX_SET_AS_RUNNABLE]    = "Transitioning to Runnable",
	[SANDBOX_RUNNABLE]           = "Runnable",
	[SANDBOX_SET_AS_RUNNING]     = "Transitioning to Running",
	[SANDBOX_RUNNING]            = "Running",
	[SANDBOX_SET_AS_PREEMPTED]   = "Transitioning to Preempted",
	[SANDBOX_PREEMPTED]          = "Preempted",
	[SANDBOX_SET_AS_BLOCKED]     = "Transitioning to Blocked",
	[SANDBOX_BLOCKED]            = "Blocked",
	[SANDBOX_SET_AS_RETURNED]    = "Transitioning to Returned",
	[SANDBOX_RETURNED]           = "Returned",
	[SANDBOX_SET_AS_COMPLETE]    = "Transitioning to Complete",
	[SANDBOX_COMPLETE]           = "Complete",
	[SANDBOX_SET_AS_ERROR]       = "Transitioning to Error",
	[SANDBOX_ERROR]              = "Error"
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
