#pragma once

#include "pretty_print.h"
#include "runtime.h"
#include "sandbox_types.h"
#include "memlogging.h"

extern FILE *sandbox_perf_log;
extern thread_local int worker_thread_idx;

/**
 * @brief Prints headers for the per-sandbox perf logs
 */
static inline void
sandbox_perf_log_print_header()
{
	if (sandbox_perf_log == NULL) { perror("sandbox perf log"); }
	fprintf(sandbox_perf_log, "id,tenant,route,state,deadline,actual,queued,uninitialized,allocated,initialized,"
	                          "runnable,interrupted,preempted,"
	                          "running_sys,running_user,asleep,returned,complete,error,proc_MHz,memory\n");
}

/**
 * Prints key performance metrics for a sandbox to sandbox_perf_log
 * This is defined by an environment variable
 * @param sandbox
 */
static inline void
sandbox_perf_log_print_entry(struct sandbox *sandbox)
{
	/* If the log was not defined by an environment variable, early out */
	if (sandbox_perf_log == NULL) return;

	uint64_t queued_duration = (sandbox->timestamp_of.dispatched - sandbox->timestamp_of.allocation) / runtime_processor_speed_MHz;

	bool miss_deadline = sandbox->timestamp_of.completion > sandbox->absolute_deadline ? true : false;
	uint64_t total_time = (sandbox->timestamp_of.completion - sandbox->timestamp_of.allocation) / runtime_processor_speed_MHz;
	uint64_t execution_time = (sandbox->duration_of_state[SANDBOX_RUNNING_SYS] + sandbox->duration_of_state[SANDBOX_RUNNING_USER])
				   / runtime_processor_speed_MHz;
	
	if (miss_deadline) {
		mem_log("tid %d %u miss deadline total time %lu execution time %lu queue %lu module name %s\n", worker_thread_idx, 
			sandbox->id, total_time, execution_time, queued_duration, sandbox->module->path);
	} else {
		mem_log("tid %d %u meet deadline total time %lu execution time %lu queue %lu module name %s\n", worker_thread_idx, 
			sandbox->id, total_time, execution_time, queued_duration, sandbox->module->path);
	}
	/*
	 * Assumption: A sandbox is never able to free pages. If linear memory management
	 * becomes more intelligent, then peak linear memory size needs to be tracked
	 * seperately from current linear memory size.
	 */
	/*mem_log("%lu,%s,%s,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%u\n",
	        sandbox->id, sandbox->tenant->name, sandbox->route->route, sandbox_state_stringify(sandbox->state),
	        sandbox->route->relative_deadline/runtime_processor_speed_MHz, sandbox->total_time/runtime_processor_speed_MHz, queued_duration,
	        sandbox->duration_of_state[SANDBOX_UNINITIALIZED]/runtime_processor_speed_MHz, sandbox->duration_of_state[SANDBOX_ALLOCATED]/runtime_processor_speed_MHz,
	        sandbox->duration_of_state[SANDBOX_INITIALIZED]/runtime_processor_speed_MHz, sandbox->duration_of_state[SANDBOX_RUNNABLE]/runtime_processor_speed_MHz,
	        sandbox->duration_of_state[SANDBOX_INTERRUPTED]/runtime_processor_speed_MHz, sandbox->duration_of_state[SANDBOX_PREEMPTED]/runtime_processor_speed_MHz,
	        sandbox->duration_of_state[SANDBOX_RUNNING_SYS]/runtime_processor_speed_MHz, sandbox->duration_of_state[SANDBOX_RUNNING_USER]/runtime_processor_speed_MHz,
	        sandbox->duration_of_state[SANDBOX_ASLEEP]/runtime_processor_speed_MHz, sandbox->duration_of_state[SANDBOX_RETURNED]/runtime_processor_speed_MHz,
	        sandbox->duration_of_state[SANDBOX_COMPLETE]/runtime_processor_speed_MHz, sandbox->duration_of_state[SANDBOX_ERROR]/runtime_processor_speed_MHz,
	        runtime_processor_speed_MHz);
	*/
}

static inline void
sandbox_perf_log_init()
{
	char *sandbox_perf_log_path = getenv("SLEDGE_SANDBOX_PERF_LOG");
	if (sandbox_perf_log_path != NULL) {
		pretty_print_key_value("Sandbox Performance Log", "%s\n", sandbox_perf_log_path);
		sandbox_perf_log = fopen(sandbox_perf_log_path, "w");
		if (sandbox_perf_log == NULL) perror("sandbox_perf_log_init\n");
		sandbox_perf_log_print_header();
	} else {
		pretty_print_key_disabled("Sandbox Performance Log");
	}
}

static inline void
sandbox_perf_log_cleanup()
{
	if (sandbox_perf_log != NULL) {
		fflush(sandbox_perf_log);
		fclose(sandbox_perf_log);
	}
}
