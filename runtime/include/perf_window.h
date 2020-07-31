#pragma once

#include <spinlock/fas.h>
#include <stdint.h>

/* Should be Power of 2! */
#define PERF_WINDOW_BUFFER_SIZE 16

#if ((PERF_WINDOW_BUFFER_SIZE == 0) || (PERF_WINDOW_BUFFER_SIZE & (PERF_WINDOW_BUFFER_SIZE - 1)) != 0)
#error "PERF_WINDOW_BUFFER_SIZE must be power of 2!"
#endif

struct perf_window {
	uint64_t          buffer[PERF_WINDOW_BUFFER_SIZE];
	uint64_t          count;
	ck_spinlock_fas_t lock;
	double            mean;
};

/**
 * Iterates through the values in the buffer and updates the mean
 * Not intended to be called directly!
 * @param self
 */
static inline void
perf_window_update_mean(struct perf_window *self)
{
	assert(self != NULL);
	assert(ck_spinlock_fas_locked(&self->lock));

	uint64_t limit = self->count;
	if (limit > PERF_WINDOW_BUFFER_SIZE) { limit = PERF_WINDOW_BUFFER_SIZE; }

	uint64_t sum = 0;
	for (uint64_t i = 0; i < limit; i++) sum += self->buffer[i];

	self->mean = (double)(sum) / limit;
};

/**
 * Iterates through the values in the buffer and updates the mean
 * Not intended to be called directly!
 * @param self
 */
static inline void
perf_window_initialize(struct perf_window *self)
{
	assert(self != NULL);

	ck_spinlock_fas_init(&self->lock);
	self->count = 0;
	self->mean  = 0;
	memset(&self->buffer, 0, sizeof(uint64_t) * PERF_WINDOW_BUFFER_SIZE);
}

/**
 * Iterates through the values in the buffer and updates the mean
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

	ck_spinlock_fas_lock(&self->lock);
	self->buffer[self->count++ % PERF_WINDOW_BUFFER_SIZE] = value;
	perf_window_update_mean(self);
	ck_spinlock_fas_unlock(&self->lock);
}

/**
 * Returns mean perf value across all executions
 * @returns mean or -1 if buffer is empty
 */
static inline double
perf_window_get_mean(struct perf_window *self)
{
	assert(self != NULL);

	if (self->count == 0) return -1;

	return self->mean;
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
