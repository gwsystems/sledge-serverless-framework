#pragma once

#include <stdio.h>

#include "debuglog.h"
#include "sandbox_summarize_page_allocations.h"
#include "sandbox_types.h"

static inline void
sandbox_summarize_page_allocations(struct sandbox *sandbox)
{
#ifdef LOG_SANDBOX_MEMORY_PROFILE
	// TODO: Handle interleavings
	char sandbox_page_allocations_log_path[100] = {};
	sandbox_page_allocations_log_path[99]       = '\0';
	snprintf(sandbox_page_allocations_log_path, 99, "%s_%d_page_allocations.csv", sandbox->module->name,
	         sandbox->module->port);

	debuglog("Logging to %s", sandbox_page_allocations_log_path);

	FILE *sandbox_page_allocations_log = fopen(sandbox_page_allocations_log_path, "a");

	fprintf(sandbox_page_allocations_log, "%lu,%lu,%s,", sandbox->id, sandbox->running_duration,
	        sandbox_state_stringify(sandbox->state));
	for (size_t i = 0; i < sandbox->page_allocation_timestamps_size; i++)
		fprintf(sandbox_page_allocations_log, "%u,", sandbox->page_allocation_timestamps[i]);

	fprintf(sandbox_page_allocations_log, "\n");
#else
	return;
#endif
}
