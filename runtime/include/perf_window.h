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
 * The sorted array sorts the last N executions by execution time
 * The buffer array acts as a circular buffer of indices into the sorted array
 *
 * This provides a sorted circular buffer
 */
struct execution_node {
	uint64_t execution_time;
	uint16_t buffer_idx; /* Reverse Index back to the sorted bin equal to this index */
};

struct perf_window {
	struct execution_node sorted[PERF_WINDOW_BUFFER_SIZE];
	uint16_t              buffer[PERF_WINDOW_BUFFER_SIZE];
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
	memset(&self->sorted, 0, sizeof(struct execution_node) * PERF_WINDOW_BUFFER_SIZE);
	memset(&self->buffer, 0, sizeof(uint16_t) * PERF_WINDOW_BUFFER_SIZE);
}


/**
 * Swaps two execution nodes in the sorted array, including updating the indices in the circular buffer
 * @param self
 * @param first_sorted_idx
 * @param second_sorted_idx
 */
static inline void
perf_window_swap(struct perf_window *self, uint16_t first_sorted_idx, uint16_t second_sorted_idx)
{
	assert(LOCK_IS_LOCKED(&self->lock));
	assert(self != NULL);
	assert(first_sorted_idx >= 0 && first_sorted_idx < PERF_WINDOW_BUFFER_SIZE);
	assert(second_sorted_idx >= 0 && second_sorted_idx < PERF_WINDOW_BUFFER_SIZE);

	uint16_t first_buffer_idx  = self->sorted[first_sorted_idx].buffer_idx;
	uint16_t second_buffer_idx = self->sorted[second_sorted_idx].buffer_idx;

	/* The execution node's buffer_idx points to a buffer cell equal to its own sorted index  */
	assert(self->buffer[first_buffer_idx] == first_sorted_idx);
	assert(self->buffer[second_buffer_idx] == second_sorted_idx);

	uint64_t first_execution_time  = self->sorted[first_sorted_idx].execution_time;
	uint64_t second_execution_time = self->sorted[second_sorted_idx].execution_time;

	/* Swap Indices in Buffer*/
	self->buffer[first_buffer_idx]  = second_sorted_idx;
	self->buffer[second_buffer_idx] = first_sorted_idx;

	/* Swap buffer_idx */
	struct execution_node tmp_node  = self->sorted[first_sorted_idx];
	self->sorted[first_sorted_idx]  = self->sorted[second_sorted_idx];
	self->sorted[second_sorted_idx] = tmp_node;

	/* The circular buffer indices should always point to the same execution times across all swaps  */
	assert(self->sorted[self->buffer[first_buffer_idx]].execution_time == first_execution_time);
	assert(self->sorted[self->buffer[second_buffer_idx]].execution_time == second_execution_time);
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

	/* A successful invocation should run for a non-zero amount of time */
	assert(value > 0);

	LOCK_LOCK(&self->lock);

	/* If count is 0, then fill entire array with initial execution times */
	if (self->count == 0) {
		for (int i = 0; i < PERF_WINDOW_BUFFER_SIZE; i++) {
			self->buffer[i] = i;
			self->sorted[i] = (struct execution_node){ .execution_time = value, .buffer_idx = i };
		}
		self->count = PERF_WINDOW_BUFFER_SIZE;
		goto done;
	}

	/* Otherwise, replace the oldest value, and then sort */
	uint16_t idx_of_oldest = self->buffer[self->count % PERF_WINDOW_BUFFER_SIZE];
	bool     check_up      = value > self->sorted[idx_of_oldest].execution_time;

	self->sorted[idx_of_oldest].execution_time = value;

	if (check_up) {
		for (uint16_t i = idx_of_oldest; i + 1 < PERF_WINDOW_BUFFER_SIZE
		                                 && self->sorted[i + 1].execution_time < self->sorted[i].execution_time;
		     i++) {
			perf_window_swap(self, i, i + 1);
		}
	} else {
		for (uint16_t i = idx_of_oldest;
		     i - 1 >= 0 && self->sorted[i - 1].execution_time > self->sorted[i].execution_time; i--) {
			perf_window_swap(self, i, i - 1);
		}
	}

	/* The idx that we replaces should still point to the same value */
	assert(self->sorted[self->buffer[self->count % PERF_WINDOW_BUFFER_SIZE]].execution_time == value);

	/* The sorted array should be ordered by execution time */
	for (int i = 1; i < PERF_WINDOW_BUFFER_SIZE; i++) {
		assert(self->sorted[i - 1].execution_time <= self->sorted[i].execution_time);
	}

	self->count++;

done:
	LOCK_UNLOCK(&self->lock);
}

/**
 * Returns pXX execution time
 * @param self
 * @param percentile represented by double between 0 and 1
 * @returns execution time or -1 if buffer is empty
 */
static inline uint64_t
perf_window_get_percentile(struct perf_window *self, double percentile)
{
	assert(self != NULL);
	assert(percentile > 0 && percentile < 1);

	if (self->count == 0) return -1;

	return self->sorted[(int)(PERF_WINDOW_BUFFER_SIZE * percentile)].execution_time;
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
