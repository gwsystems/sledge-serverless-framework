#pragma once

#include <stdint.h>

#include "lock.h"
#include "runtime.h"
#include "worker_thread.h"

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

/**
 * Initializes perf window
 * @param self
 */
static inline void
perf_window_initialize(struct perf_window *self)
{
	assert(self != NULL);

	LOCK_INIT(&self->lock);
	self->count = 0;
	memset(&self->by_duration, 0, sizeof(struct execution_node) * PERF_WINDOW_BUFFER_SIZE);
	memset(&self->by_termination, 0, sizeof(uint16_t) * PERF_WINDOW_BUFFER_SIZE);
}


/**
 * Swaps two execution nodes in the by_duration array, including updating the indices in the by_termination circular
 * buffer
 * @param self
 * @param first_by_duration_idx
 * @param second_by_duration_idx
 */
static inline void
perf_window_swap(struct perf_window *self, uint16_t first_by_duration_idx, uint16_t second_by_duration_idx)
{
	assert(LOCK_IS_LOCKED(&self->lock));
	assert(self != NULL);
	assert(first_by_duration_idx >= 0 && first_by_duration_idx < PERF_WINDOW_BUFFER_SIZE);
	assert(second_by_duration_idx >= 0 && second_by_duration_idx < PERF_WINDOW_BUFFER_SIZE);

	uint16_t first_by_termination_idx  = self->by_duration[first_by_duration_idx].by_termination_idx;
	uint16_t second_by_termination_idx = self->by_duration[second_by_duration_idx].by_termination_idx;

	/* The execution node's by_termination_idx points to a by_termination cell equal to its own by_duration index */
	assert(self->by_termination[first_by_termination_idx] == first_by_duration_idx);
	assert(self->by_termination[second_by_termination_idx] == second_by_duration_idx);

	uint64_t first_execution_time  = self->by_duration[first_by_duration_idx].execution_time;
	uint64_t second_execution_time = self->by_duration[second_by_duration_idx].execution_time;

	/* Swap Indices in Buffer*/
	self->by_termination[first_by_termination_idx]  = second_by_duration_idx;
	self->by_termination[second_by_termination_idx] = first_by_duration_idx;

	/* Swap by_termination_idx */
	struct execution_node tmp_node            = self->by_duration[first_by_duration_idx];
	self->by_duration[first_by_duration_idx]  = self->by_duration[second_by_duration_idx];
	self->by_duration[second_by_duration_idx] = tmp_node;

	/* The circular by_termination indices should always point to the same execution times across all swaps  */
	assert(self->by_duration[self->by_termination[first_by_termination_idx]].execution_time
	       == first_execution_time);
	assert(self->by_duration[self->by_termination[second_by_termination_idx]].execution_time
	       == second_execution_time);
}

/**
 * Adds a new value to the perf window
 * Not intended to be called directly!
 * @param self
 * @param value
 */
static inline void
perf_window_add(struct perf_window *self, uint64_t value)
{
	assert(self != NULL);

	if (unlikely(!LOCK_IS_LOCKED(&self->lock))) panic("lock not held when calling perf_window_add\n");

	/* A successful invocation should run for a non-zero amount of time */
	assert(value > 0);

	/* If count is 0, then fill entire array with initial execution times */
	if (self->count == 0) {
		for (int i = 0; i < PERF_WINDOW_BUFFER_SIZE; i++) {
			self->by_termination[i] = i;
			self->by_duration[i]    = (struct execution_node){ .execution_time     = value,
                                                                        .by_termination_idx = i };
		}
		self->count = PERF_WINDOW_BUFFER_SIZE;
		goto done;
	}

	/* Otherwise, replace the oldest value, and then sort */
	uint16_t idx_of_oldest = self->by_termination[self->count % PERF_WINDOW_BUFFER_SIZE];
	bool     check_up      = value > self->by_duration[idx_of_oldest].execution_time;

	self->by_duration[idx_of_oldest].execution_time = value;

	if (check_up) {
		for (uint16_t i = idx_of_oldest;
		     i + 1 < PERF_WINDOW_BUFFER_SIZE
		     && self->by_duration[i + 1].execution_time < self->by_duration[i].execution_time;
		     i++) {
			perf_window_swap(self, i, i + 1);
		}
	} else {
		for (int i = idx_of_oldest;
		     i - 1 >= 0 && self->by_duration[i - 1].execution_time > self->by_duration[i].execution_time; i--) {
			perf_window_swap(self, i, i - 1);
		}
	}

	/* The idx that we replaces should still point to the same value */
	assert(self->by_duration[self->by_termination[self->count % PERF_WINDOW_BUFFER_SIZE]].execution_time == value);

/* The by_duration array should be ordered by execution time */
#ifndef NDEBUG
	for (int i = 1; i < PERF_WINDOW_BUFFER_SIZE; i++) {
		assert(self->by_duration[i - 1].execution_time <= self->by_duration[i].execution_time);
	}
#endif

	self->count++;

done:
	return;
}

/**
 * Returns pXX execution time
 * @param self
 * @param percentile represented by int between 50 and 99
 * @param precomputed_index memoized index for quick lookup when by_duration is full
 * @returns execution time
 */
static inline uint64_t
perf_window_get_percentile(struct perf_window *self, int percentile, int precomputed_index)
{
	assert(self != NULL);
	assert(percentile >= 50 && percentile <= 99);
	int size = self->count;
	assert(size > 0);

	if (likely(size >= PERF_WINDOW_BUFFER_SIZE)) return self->by_duration[precomputed_index].execution_time;

	return self->by_duration[size * percentile / 100].execution_time;
}

/**
 * Returns the total count of executions
 * @returns total count
 */
static inline uint64_t
perf_window_get_count(struct perf_window *self)
{
	assert(self != NULL);

	return self->count;
}
