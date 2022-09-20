#include "admissions_control.h"
#include "admissions_info.h"
#include "debuglog.h"
#include "perf_window.h"

/**
 * Initializes perf window
 * @param admissions_info
 */
void
admissions_info_initialize(struct admissions_info *admissions_info, uint8_t percentile, uint64_t expected_execution,
                           uint64_t relative_deadline)
{
#ifdef ADMISSIONS_CONTROL
	assert(relative_deadline > 0);
	assert(expected_execution > 0);
	admissions_info->relative_deadline = relative_deadline;
	admissions_info->estimate = admissions_control_calculate_estimate(expected_execution, relative_deadline);
	debuglog("Initial Estimate: %lu\n", admissions_info->estimate);
	assert(admissions_info != NULL);

	perf_window_initialize(&admissions_info->perf_window);

	if (unlikely(percentile < 50 || percentile > 99)) panic("Invalid admissions percentile");
	admissions_info->percentile = percentile;

	admissions_info->control_index = PERF_WINDOW_CAPACITY * percentile / 100;
#ifdef LOG_ADMISSIONS_CONTROL
	debuglog("Percentile: %u\n", admissions_info->percentile);
	debuglog("Control Index: %d\n", admissions_info->control_index);
#endif
#endif
}


/*
 * Adds an execution value to the perf window and calculates and caches and updated estimate
 * @param admissions_info
 * @param execution_duration
 */
void
admissions_info_update(struct admissions_info *admissions_info, uint64_t execution_duration)
{
#ifdef ADMISSIONS_CONTROL
	struct perf_window *perf_window = &admissions_info->perf_window;

	lock_node_t node = {};
	lock_lock(&perf_window->lock, &node);
	perf_window_add(perf_window, execution_duration);
	uint64_t estimated_execution = perf_window_get_percentile(perf_window, admissions_info->percentile,
	                                                          admissions_info->control_index);
	admissions_info->estimate    = admissions_control_calculate_estimate(estimated_execution,
	                                                                     admissions_info->relative_deadline);
	lock_unlock(&perf_window->lock, &node);
#endif
}
