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

	lock_init(&perf_window->lock);
	perf_window->count = 0;
	memset(perf_window->by_duration, 0, sizeof(struct execution_node) * perf_window_capacity);
	memset(perf_window->by_termination, 0, sizeof(uint16_t) * perf_window_capacity);
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
	assert(lock_is_locked(&perf_window->lock));
	assert(perf_window != NULL);
	assert(first_by_duration_idx < perf_window_capacity);
	assert(second_by_duration_idx < perf_window_capacity);

	uint16_t first_by_termination_idx  = perf_window->by_duration[first_by_duration_idx].by_termination_idx;
	uint16_t second_by_termination_idx = perf_window->by_duration[second_by_duration_idx].by_termination_idx;

	/* The execution node's by_termination_idx points to a by_termination cell equal to its own by_duration index */
	assert(perf_window->by_termination[first_by_termination_idx] == first_by_duration_idx);
	assert(perf_window->by_termination[second_by_termination_idx] == second_by_duration_idx);

	uint64_t first_execution_time  = perf_window->by_duration[first_by_duration_idx].execution_time;
	uint64_t second_execution_time = perf_window->by_duration[second_by_duration_idx].execution_time;

	/* Swap indices */
	perf_window->by_termination[first_by_termination_idx]  = second_by_duration_idx;
	perf_window->by_termination[second_by_termination_idx] = first_by_duration_idx;

	/* Swap nodes */
	struct execution_node tmp_node                   = perf_window->by_duration[first_by_duration_idx];
	perf_window->by_duration[first_by_duration_idx]  = perf_window->by_duration[second_by_duration_idx];
	perf_window->by_duration[second_by_duration_idx] = tmp_node;

	/* The circular by_termination indices should always point to the same execution times across all swaps  */
	assert(perf_window->by_duration[perf_window->by_termination[first_by_termination_idx]].execution_time
	       == first_execution_time);
	assert(perf_window->by_duration[perf_window->by_termination[second_by_termination_idx]].execution_time
	       == second_execution_time);
}

static inline void
perf_window_fill(struct perf_window *perf_window, uint64_t newest_execution_time)
{
	for (uint16_t i = 0; i < perf_window_capacity; i++) {
		perf_window->by_termination[i] = i;
		perf_window->by_duration[i]    = (struct execution_node){ .execution_time     = newest_execution_time,
			                                                  .by_termination_idx = i };
	}
	perf_window->count = perf_window_capacity;
}

/**
 * Adds newest_execution_time to the perf window
 * Not intended to be called directly!
 * @param perf_window
 * @param newest_execution_time
 */
static inline void
perf_window_add(struct perf_window *perf_window, uint64_t newest_execution_time)
{
	assert(perf_window != NULL);
	/* Assumption: A successful invocation should run for a non-zero amount of time */
	assert(newest_execution_time > 0);

	uint16_t idx_to_replace;
	uint64_t previous_execution_time;
	bool     check_up;

	if (unlikely(!lock_is_locked(&perf_window->lock))) panic("lock not held when calling perf_window_add\n");

	/* If perf window is empty, fill all elements with newest_execution_time */
	if (perf_window->count == 0) {
		perf_window_fill(perf_window, newest_execution_time);
		goto done;
	}

	/* If full, replace the oldest execution_time. Save the old execution time to know which direction to swap */
	idx_to_replace          = perf_window->by_termination[perf_window->count % perf_window_capacity];
	previous_execution_time = perf_window->by_duration[idx_to_replace].execution_time;
	perf_window->by_duration[idx_to_replace].execution_time = newest_execution_time;

	/* At this point, the by_duration array is partially sorted. The node we overwrote needs to be shifted left or
	 * right. We can determine which direction to shift by comparing with the previous execution time.  */
	if (newest_execution_time > previous_execution_time) {
		for (uint16_t i = idx_to_replace;
		     i + 1 < perf_window_capacity
		     && perf_window->by_duration[i + 1].execution_time < perf_window->by_duration[i].execution_time;
		     i++) {
			perf_window_swap(perf_window, i, i + 1);
		}
	} else {
		for (uint16_t i = idx_to_replace;
		     i >= 1
		     && perf_window->by_duration[i - 1].execution_time > perf_window->by_duration[i].execution_time;
		     i--) {
			perf_window_swap(perf_window, i, i - 1);
		}
	}

	/* The idx that we replaces should still point to the same newest_execution_time */
	assert(perf_window->by_duration[perf_window->by_termination[perf_window->count % perf_window_capacity]]
	         .execution_time
	       == newest_execution_time);

/* The by_duration array should be ordered by execution time */
#ifndef NDEBUG
	for (int i = 1; i < perf_window_capacity; i++) {
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
perf_window_get_percentile(struct perf_window *perf_window, uint8_t percentile, int precomputed_index)
{
	assert(perf_window != NULL);
	assert(percentile >= 50 && percentile <= 99);

	if (unlikely(perf_window->count == 0)) return 0;

	int idx = precomputed_index;
	if (unlikely(perf_window->count < perf_window_capacity)) idx = perf_window->count * percentile / 100;

	return perf_window->by_duration[idx].execution_time;
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
