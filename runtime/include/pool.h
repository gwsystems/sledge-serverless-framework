#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "generic_thread.h"
#include "lock.h"
#include "ps_list.h"
#include "wasm_memory.h"

struct pool {
	bool                use_lock;
	lock_t              lock;
	struct ps_list_head list;
};

static inline bool
pool_is_empty(struct pool *self)
{
	assert(self != NULL);

	return ps_list_head_empty(&self->list);
}

static inline void
pool_init(struct pool *self, bool use_lock)
{
	ps_list_head_init(&self->list);
	self->use_lock = use_lock;
	if (use_lock) LOCK_INIT(&self->lock);
}

static inline void
pool_deinit(struct pool *self)
{
	if (pool_is_empty(self)) return;

	struct wasm_memory *iterator = NULL;
	struct wasm_memory *buffer   = NULL;

	ps_list_foreach_del_d(&self->list, iterator, buffer)
	{
		ps_list_rem_d(iterator);
		wasm_memory_free(iterator);
	}
}

static inline struct wasm_memory *
pool_remove_nolock(struct pool *self)
{
	assert(self != NULL);
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));

	struct wasm_memory *obj = NULL;

	if (pool_is_empty(self)) return obj;

	obj = ps_list_head_first_d(&self->list, struct wasm_memory);
	assert(obj);
	ps_list_rem_d(obj);

	return obj;
}

static inline struct wasm_memory *
pool_remove(struct pool *self)
{
	assert(self != NULL);
	assert(self->use_lock);

	struct wasm_memory *obj = NULL;

	if (pool_is_empty(self)) return obj;

	LOCK_LOCK(&self->lock);
	if (pool_is_empty(self)) {
		LOCK_UNLOCK(&self->lock);
		return obj;
	}

	obj = ps_list_head_first_d(&self->list, struct wasm_memory);
	assert(obj);
	ps_list_rem_d(obj);
	LOCK_UNLOCK(&self->lock);
	return obj;
}

static inline int
pool_add_nolock(struct pool *self, struct wasm_memory *obj)
{
	assert(self != NULL);
	assert(obj != NULL);
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));

	ps_list_head_add_d(&self->list, obj);
	return 0;
}

static inline int
pool_add(struct pool *self, struct wasm_memory *obj)
{
	assert(self != NULL);
	assert(obj != NULL);
	assert(self->use_lock);

	LOCK_LOCK(&self->lock);
	ps_list_head_add_d(&self->list, obj);
	LOCK_UNLOCK(&self->lock);
	return 0;
}

static inline void
pool_free(struct pool *self)
{
	while (!pool_is_empty(self)) free(pool_remove(self));

	free(self);
}
