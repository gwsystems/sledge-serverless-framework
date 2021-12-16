#pragma once

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "lock.h"
#include "panic.h"
#include "perf_window_t.h"
#include "runtime.h"
#include "worker_thread.h"

/**
 * Initializes perf window
 * @param perf_window
 */
static inline void
perf_window_initialize(struct perf_window *perf_window)
{
	assert(perf_window != NULL);

	LOCK_INIT(&perf_window->lock);
	perf_window->count = 0;
	memset(&perf_window->by_duration, 0, sizeof(struct execution_node) * PERF_WINDOW_BUFFER_SIZE);
	memset(&perf_window->by_termination, 0, sizeof(uint16_t) * PERF_WINDOW_BUFFER_SIZE);
}


/**
 * Swaps two execution nodes in the by_duration array, including updating the indices in the by_termination circular
 * buffer
 * @param perf_window
 * @param first_by_duration_idx
 * @param second_by_duration_idx
 */
static inline void
perf_window_swap(struct perf_window *perf_window, uint16_t first_by_duration_idx, uint16_t second_by_duration_idx)
{
	assert(LOCK_IS_LOCKED(&perf_window->lock));
	assert(perf_window != NULL);
	assert(first_by_duration_idx >= 0 && first_by_duration_idx < PERF_WINDOW_BUFFER_SIZE);
	assert(second_by_duration_idx >= 0 && second_by_duration_idx < PERF_WINDOW_BUFFER_SIZE);

	uint16_t first_by_termination_idx  = perf_window->by_duration[first_by_duration_idx].by_termination_idx;
	uint16_t second_by_termination_idx = perf_window->by_duration[second_by_duration_idx].by_termination_idx;

	/* The execution node's by_termination_idx points to a by_termination cell equal to its own by_duration index */
	assert(perf_window->by_termination[first_by_termination_idx] == first_by_duration_idx);
	assert(perf_window->by_termination[second_by_termination_idx] == second_by_duration_idx);

	uint64_t first_execution_time  = perf_window->by_duration[first_by_duration_idx].execution_time;
	uint64_t second_execution_time = perf_window->by_duration[second_by_duration_idx].execution_time;

	/* Swap Indices in Buffer*/
	perf_window->by_termination[first_by_termination_idx]  = second_by_duration_idx;
	perf_window->by_termination[second_by_termination_idx] = first_by_duration_idx;

	/* Swap by_termination_idx */
	struct execution_node tmp_node            = perf_window->by_duration[first_by_duration_idx];
	perf_window->by_duration[first_by_duration_idx]  = perf_window->by_duration[second_by_duration_idx];
	perf_window->by_duration[second_by_duration_idx] = tmp_node;

	/* The circular by_termination indices should always point to the same execution times across all swaps  */
	assert(perf_window->by_duration[perf_window->by_termination[first_by_termination_idx]].execution_time
	       == first_execution_time);
	assert(perf_window->by_duration[perf_window->by_termination[second_by_termination_idx]].execution_time
	       == second_execution_time);
}

/**
 * Adds a new value to the perf window
 * Not intended to be called directly!
 * @param perf_window
 * @param value
 */
static inline void
perf_window_add(struct perf_window *perf_window, uint64_t value)
{
	assert(perf_window != NULL);

	uint16_t idx_of_oldest;
	bool     check_up;

	if (unlikely(!LOCK_IS_LOCKED(&perf_window->lock))) panic("lock not held when calling perf_window_add\n");

	/* A successful invocation should run for a non-zero amount of time */
	assert(value > 0);

	/* If count is 0, then fill entire array with initial execution times */
	if (perf_window->count == 0) {
		for (int i = 0; i < PERF_WINDOW_BUFFER_SIZE; i++) {
			perf_window->by_termination[i] = i;
			perf_window->by_duration[i]    = (struct execution_node){ .execution_time     = value,
                                                                        .by_termination_idx = i };
		}
		perf_window->count = PERF_WINDOW_BUFFER_SIZE;
		goto done;
	}

	/* Otherwise, replace the oldest value, and then sort */
	idx_of_oldest = perf_window->by_termination[perf_window->count % PERF_WINDOW_BUFFER_SIZE];
	check_up      = value > perf_window->by_duration[idx_of_oldest].execution_time;

	perf_window->by_duration[idx_of_oldest].execution_time = value;

	if (check_up) {
		for (uint16_t i = idx_of_oldest;
		     i + 1 < PERF_WINDOW_BUFFER_SIZE
		     && perf_window->by_duration[i + 1].execution_time < perf_window->by_duration[i].execution_time;
		     i++) {
			perf_window_swap(perf_window, i, i + 1);
		}
	} else {
		for (int i = idx_of_oldest;
		     i - 1 >= 0 && perf_window->by_duration[i - 1].execution_time > perf_window->by_duration[i].execution_time; i--) {
			perf_window_swap(perf_window, i, i - 1);
		}
	}

	/* The idx that we replaces should still point to the same value */
	assert(perf_window->by_duration[perf_window->by_termination[perf_window->count % PERF_WINDOW_BUFFER_SIZE]].execution_time == value);

/* The by_duration array should be ordered by execution time */
#ifndef NDEBUG
	for (int i = 1; i < PERF_WINDOW_BUFFER_SIZE; i++) {
		assert(perf_window->by_duration[i - 1].execution_time <= perf_window->by_duration[i].execution_time);
	}
#endif

	perf_window->count++;

done:
	return;
}

/**
 * Returns pXX execution time
 * @param perf_window
 * @param percentile represented by int between 50 and 99
 * @param precomputed_index memoized index for quick lookup when by_duration is full
 * @returns execution time
 */
static inline uint64_t
perf_window_get_percentile(struct perf_window *perf_window, int percentile, int precomputed_index)
{
	assert(perf_window != NULL);
	assert(percentile >= 50 && percentile <= 99);
	int size = perf_window->count;
	assert(size > 0);

	if (likely(size >= PERF_WINDOW_BUFFER_SIZE)) return perf_window->by_duration[precomputed_index].execution_time;

	return perf_window->by_duration[size * percentile / 100].execution_time;
}

/**
 * Returns the total count of executions
 * @returns total count
 */
static inline uint64_t
perf_window_get_count(struct perf_window *perf_window)
{
	assert(perf_window != NULL);

	return perf_window->count;
}
