#pragma once

#include "perf_window_t.h"

struct execution_histogram {
	struct perf_window perf_window;
	uint8_t            percentile;          /* 50 - 99 */
	int                control_index;       /* Precomputed Lookup index when perf_window is full */
	uint64_t           estimated_execution; /* cycles */
};

void execution_histogram_initialize(struct execution_histogram *execution_histogram, uint8_t percentile,
                                    uint64_t expected_execution);
void execution_histogram_update(struct execution_histogram *execution_histogram, uint64_t execution_duration);
