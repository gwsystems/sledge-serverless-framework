#ifdef EXECUTION_HISTOGRAM

#include "execution_histogram.h"
#include "debuglog.h"
#include "perf_window.h"

/**
 * Initializes execution_histogram and its perf window
 * @param execution_histogram
 */
void
execution_histogram_initialize(struct execution_histogram *execution_histogram, uint8_t percentile,
                               uint64_t expected_execution)
{
	assert(expected_execution > 0);
	execution_histogram->estimated_execution = expected_execution;

	assert(execution_histogram != NULL);
	perf_window_initialize(&execution_histogram->perf_window);

	if (unlikely(percentile < 50 || percentile > 99)) panic("Invalid percentile");
	execution_histogram->percentile    = percentile;
	execution_histogram->control_index = PERF_WINDOW_CAPACITY * percentile / 100;
#ifdef LOG_EXECUTION_HISTOGRAM
	debuglog("Percentile: %u\n", execution_histogram->percentile);
	debuglog("Control Index: %d\n", execution_histogram->control_index);
#endif
}


/*
 * Adds an execution value to the perf window
 * @param execution_histogram
 * @param execution_duration
 */
void
execution_histogram_update(struct execution_histogram *execution_histogram, uint64_t execution_duration)
{
	struct perf_window *perf_window = &execution_histogram->perf_window;

	lock_node_t node = {};
	lock_lock(&perf_window->lock, &node);
	perf_window_add(perf_window, execution_duration);
	uint64_t estimated_execution = perf_window_get_percentile(perf_window, execution_histogram->percentile,
	                                                          execution_histogram->control_index);
	execution_histogram->estimated_execution = estimated_execution;
	lock_unlock(&perf_window->lock, &node);
}

#endif