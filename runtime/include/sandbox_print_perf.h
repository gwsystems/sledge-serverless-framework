#pragma once

#include "runtime.h"
#include "sandbox_types.h"

/**
 * Prints key performance metrics for a sandbox to runtime_sandbox_perf_log
 * This is defined by an environment variable
 * @param sandbox
 */
static inline void
sandbox_print_perf(struct sandbox *sandbox)
{
	/* If the log was not defined by an environment variable, early out */
	if (runtime_sandbox_perf_log == NULL) return;

	uint64_t queued_duration = sandbox->timestamp_of.allocation - sandbox->timestamp_of.request_arrival;

	/*
	 * Assumption: A sandbox is never able to free pages. If linear memory management
	 * becomes more intelligent, then peak linear memory size needs to be tracked
	 * seperately from current linear memory size.
	 */
	fprintf(runtime_sandbox_perf_log,
	        "%lu,%s,%d,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%u,%u\n", sandbox->id,
	        sandbox->module->name, sandbox->module->port, sandbox_state_stringify(sandbox->state),
	        sandbox->module->relative_deadline, sandbox->total_time, queued_duration,
	        sandbox->duration_of_state[SANDBOX_UNINITIALIZED], sandbox->duration_of_state[SANDBOX_ALLOCATED],
	        sandbox->duration_of_state[SANDBOX_INITIALIZED], sandbox->duration_of_state[SANDBOX_RUNNABLE],
	        sandbox->duration_of_state[SANDBOX_PREEMPTED], sandbox->duration_of_state[SANDBOX_RUNNING_SYS],
	        sandbox->duration_of_state[SANDBOX_RUNNING_USER], sandbox->duration_of_state[SANDBOX_ASLEEP],
	        sandbox->duration_of_state[SANDBOX_RETURNED], sandbox->duration_of_state[SANDBOX_COMPLETE],
	        sandbox->duration_of_state[SANDBOX_ERROR], runtime_processor_speed_MHz, sandbox->memory.size);
}
