#pragma once

#include <stdint.h>
#include <assert.h>

#include "lock.h"
#include "perf_window_t.h"
#include "runtime.h"
#include "worker_thread.h"
#include "memlogging.h"
#include "panic.h"

/**
 * Initializes perf window
 * @param self
 */
static inline void
perf_window_initialize(struct perf_window *self, char* module_name)
{
	assert(self != NULL);

	LOCK_INIT(&self->lock);
	strncpy(self->name, module_name, 32);
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
	//assert(percentile >= 50 && percentile <= 99);
	int size = self->count;
	//assert(size > 0);
	if (size == 0) {
		return 0;
	}

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

/**
 * Print the items in the perf window
 */
static inline void
perf_window_print(struct perf_window *self) 
{
	assert(self != NULL);
	if (self->count % PERF_WINDOW_BUFFER_SIZE != 0) { return; }
	/* Not need to hold lock because this operation won't add/delete the array */
	float min = self->by_duration[0].execution_time/1000.0;
	float max = self->by_duration[PERF_WINDOW_BUFFER_SIZE-1].execution_time/1000.0;
	uint64_t sum = 0;
        float fifty_p = self->by_duration[PERF_WINDOW_BUFFER_SIZE * 50 / 100].execution_time/1000.0;	
        float seventy_p = self->by_duration[PERF_WINDOW_BUFFER_SIZE * 70 / 100].execution_time/1000.0;	
        float eighty_p = self->by_duration[PERF_WINDOW_BUFFER_SIZE * 80 / 100].execution_time/1000.0;	
        float nighty_p = self->by_duration[PERF_WINDOW_BUFFER_SIZE * 90 / 100].execution_time/1000.0;	
        float nighty_night_p = self->by_duration[PERF_WINDOW_BUFFER_SIZE * 99 / 100].execution_time/1000.0;	

	/*mem_log("module %s perf window:\n", self->name);
	for (int i = 0; i < PERF_WINDOW_BUFFER_SIZE; i++) {
		sum += self->by_duration[i].execution_time;
                mem_log("%f,", self->by_duration[i].execution_time/1000.0);
        }
	mem_log("\n");
	*/
	float avg = (sum/(float)PERF_WINDOW_BUFFER_SIZE)/1000.0;
	mem_log("min:%f,max:%f,fifty_p:%f,seventy_p:%f,eighty_p:%f,nighty_p:%f,nighty_night_p:%f,avg:%f\n", min,max,fifty_p,seventy_p,eighty_p,nighty_p,nighty_night_p, avg);
}
