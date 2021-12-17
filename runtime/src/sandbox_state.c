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

#ifdef SANDBOX_STATE_TOTALS
_Atomic uint32_t sandbox_state_totals[SANDBOX_STATE_COUNT];
#endif
