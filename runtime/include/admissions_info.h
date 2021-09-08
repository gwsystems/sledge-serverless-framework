#pragma once

#include "perf_window_t.h"

struct admissions_info {
	struct perf_window perf_window;
	int                percentile;        /* 50 - 99 */
	int                control_index;     /* Precomputed Lookup index when perf_window is full */
	uint64_t           estimate;          /* cycles */
	uint64_t           relative_deadline; /* Relative deadline in cycles. This is duplicated state */
};

void admissions_info_initialize(struct admissions_info *self, char* module_name, int percentile, uint64_t expected_execution,
                                uint64_t relative_deadline);
void admissions_info_update(struct admissions_info *self, uint64_t execution_duration);

uint64_t admission_info_get_percentile(struct admissions_info *self);

