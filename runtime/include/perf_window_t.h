#pragma once

#include <stdint.h>

#include "lock.h"

/* Should be Power of 2! */
#define PERF_WINDOW_BUFFER_SIZE 16

#if ((PERF_WINDOW_BUFFER_SIZE == 0) || (PERF_WINDOW_BUFFER_SIZE & (PERF_WINDOW_BUFFER_SIZE - 1)) != 0)
#error "PERF_WINDOW_BUFFER_SIZE must be power of 2!"
#endif

/*
 * The by_duration array sorts the last N executions by execution time
 * The by_termination array acts as a circular buffer that maps to indices in the by_duration array
 *
 * by_termination ensures that the when the circular buffer is full, the oldest data in both arrays is
 * overwritten, providing a sorted circular buffer
 */
struct execution_node {
	uint64_t execution_time;
	uint16_t by_termination_idx; /* Reverse idx of the associated by_termination bin. Used for swaps! */
};

struct perf_window {
	struct execution_node by_duration[PERF_WINDOW_BUFFER_SIZE];
	uint16_t              by_termination[PERF_WINDOW_BUFFER_SIZE];
	uint64_t              count;
	lock_t                lock;
};
