#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "generic_thread.h"
#include "lock.h"

struct pool {
	bool    use_lock;
	lock_t  lock;
	ssize_t top;
	size_t  capacity;
	void *  buffer[];
};

static inline bool
pool_is_empty(struct pool *self)
{
	return self->top == -1;
}

static inline bool
pool_is_full(struct pool *self)
{
	return self->top + 1 == self->capacity;
}

static inline void *
pool_allocate_object_nolock(struct pool *self)
{
	assert(self != NULL);
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));

	void *result = NULL;

	if (pool_is_empty(self)) return result;

	result = self->buffer[self->top--];
	return result;
}

static inline void *
pool_allocate_object(struct pool *self)
{
	assert(self != NULL);
	assert(self->use_lock);

	void *result = NULL;

	if (pool_is_empty(self)) return result;

	LOCK_LOCK(&self->lock);
	if (pool_is_empty(self)) {
		LOCK_UNLOCK(&self->lock);
		return result;
	}

	result = self->buffer[self->top--];
	assert(result != NULL);
	LOCK_UNLOCK(&self->lock);
	return result;
}

static inline int
pool_free_object_nolock(struct pool *self, void *obj)
{
	assert(self != NULL);
	assert(obj != NULL);
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));

	if (pool_is_full(self)) return -1;

	self->buffer[++self->top] = obj;
	return 0;
}

static inline int
pool_free_object(struct pool *self, void *obj)
{
	assert(self != NULL);
	assert(obj != NULL);
	assert(self->use_lock);

	if (pool_is_full(self)) return -1;

	LOCK_LOCK(&self->lock);
	if (pool_is_full(self)) {
		LOCK_UNLOCK(&self->lock);
		return -1;
	}

	self->buffer[++self->top] = obj;
	LOCK_UNLOCK(&self->lock);
	return 0;
}

static inline struct pool *
pool_init(size_t capacity, bool use_lock)
{
	struct pool *self = (struct pool *)calloc(1, sizeof(struct pool) + capacity * sizeof(void *));
	self->top         = -1;
	self->capacity    = capacity;
	self->use_lock    = use_lock;

	if (use_lock) LOCK_INIT(&self->lock);

	return self;
}

static inline void
pool_free(struct pool *self)
{
	while (!pool_is_empty(self)) free(pool_allocate_object(self));

	free(self);
}
