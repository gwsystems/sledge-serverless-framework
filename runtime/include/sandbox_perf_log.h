#pragma once

#include "pretty_print.h"
#include "runtime.h"
#include "sandbox_types.h"

extern FILE *sandbox_perf_log;

/**
 * @brief Prints headers for the per-sandbox perf logs
 */
static inline void
sandbox_perf_log_print_header()
{
	if (sandbox_perf_log == NULL) { perror("sandbox perf log"); }
	fprintf(sandbox_perf_log,
	        "id,tenant,route,state,deadline,actual,queued,uninitialized,allocated,initialized,"
	        "runnable,interrupted,preempted,"
	        "running_sys,running_user,asleep,returned,complete,error,proc_MHz,response_code,guarantee_type,payload_size\n");
}

/**
 * Prints key performance metrics for a denied request (by AC) to perf_log
 * This is defined by an environment variable
 * @param module
 */
static inline void
sandbox_perf_log_print_denied_entry(struct tenant *tenant, struct route *route, uint16_t response_code)
{
	/* If the log was not defined by an environment variable, early out */
	if (sandbox_perf_log == NULL) return;

	fprintf(sandbox_perf_log, "-1,%s,%s,Deny,%lu,0,0,0,0,0,0,0,0,0,0,0,0,0,0,%u,%u,0,0\n", tenant->name, route->route,
	        route->relative_deadline, runtime_processor_speed_MHz, response_code);
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

	uint64_t queued_duration = sandbox->timestamp_of.dispatched - sandbox->timestamp_of.allocation; // TODO: Consider writeback

	/*
	 * Assumption: A sandbox is never able to free pages. If linear memory management
	 * becomes more intelligent, then peak linear memory size needs to be tracked
	 * seperately from current linear memory size.
	 */
	fprintf(sandbox_perf_log, "%lu,%s,%s,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%u,%u,%d,%d\n",
	        sandbox->id, sandbox->tenant->name, sandbox->route->route, sandbox_state_stringify(sandbox->state),
	        sandbox->route->relative_deadline, sandbox->total_time, queued_duration,
	        sandbox->duration_of_state[SANDBOX_UNINITIALIZED], sandbox->duration_of_state[SANDBOX_ALLOCATED],
	        sandbox->duration_of_state[SANDBOX_INITIALIZED], sandbox->duration_of_state[SANDBOX_RUNNABLE],
	        sandbox->duration_of_state[SANDBOX_INTERRUPTED], sandbox->duration_of_state[SANDBOX_PREEMPTED],
	        sandbox->duration_of_state[SANDBOX_RUNNING_SYS], sandbox->duration_of_state[SANDBOX_RUNNING_USER],
	        sandbox->duration_of_state[SANDBOX_ASLEEP], sandbox->duration_of_state[SANDBOX_RETURNED],
	        sandbox->duration_of_state[SANDBOX_COMPLETE], sandbox->duration_of_state[SANDBOX_ERROR],
	        runtime_processor_speed_MHz, sandbox->response_code, sandbox->global_queue_type, sandbox->payload_size);
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
