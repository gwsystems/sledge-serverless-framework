#pragma once

#include <assert.h>
#include <stdint.h>

#include "lock.h"

enum
{
	PERF_WINDOW_CAPACITY = 256
};

static_assert(PERF_WINDOW_CAPACITY && !(PERF_WINDOW_CAPACITY & (PERF_WINDOW_CAPACITY - 1)),
              "PERF_WINDOW_CAPACITY must be power of 2!");

static_assert(PERF_WINDOW_CAPACITY <= UINT16_MAX, "PERF_WINDOW_CAPACITY must be indexable by a 16-bit unsigned int");

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
	struct execution_node by_duration[PERF_WINDOW_CAPACITY];
	uint16_t              by_termination[PERF_WINDOW_CAPACITY];
	uint64_t              count;
	lock_t                lock;
};
