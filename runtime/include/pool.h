#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "generic_thread.h"
#include "lock.h"
#include "ps_list.h"

#define INIT_POOL(STRUCT_NAME, DTOR_FN)                                                                            \
	struct STRUCT_NAME##_pool {                                                                                \
		bool                use_lock;                                                                      \
		lock_t              lock;                                                                          \
		struct ps_list_head list;                                                                          \
	};                                                                                                         \
                                                                                                                   \
	static inline bool STRUCT_NAME##_pool_is_empty(struct STRUCT_NAME##_pool *self)                            \
	{                                                                                                          \
		assert(self != NULL);                                                                              \
                                                                                                                   \
		return ps_list_head_empty(&self->list);                                                            \
	}                                                                                                          \
                                                                                                                   \
	static inline void STRUCT_NAME##_pool_init(struct STRUCT_NAME##_pool *self, bool use_lock)                 \
	{                                                                                                          \
		ps_list_head_init(&self->list);                                                                    \
		self->use_lock = use_lock;                                                                         \
		if (use_lock) LOCK_INIT(&self->lock);                                                              \
	}                                                                                                          \
                                                                                                                   \
	static inline void STRUCT_NAME##_pool_deinit(struct STRUCT_NAME##_pool *self)                              \
	{                                                                                                          \
		if (STRUCT_NAME##_pool_is_empty(self)) return;                                                     \
		struct STRUCT_NAME *iterator = NULL;                                                               \
		struct STRUCT_NAME *buffer   = NULL;                                                               \
		ps_list_foreach_del_d(&self->list, iterator, buffer)                                               \
		{                                                                                                  \
			ps_list_rem_d(iterator);                                                                   \
			DTOR_FN(iterator);                                                                         \
		}                                                                                                  \
	}                                                                                                          \
                                                                                                                   \
	static inline struct STRUCT_NAME *STRUCT_NAME##_pool_remove_nolock(struct STRUCT_NAME##_pool *self)        \
	{                                                                                                          \
		assert(self != NULL);                                                                              \
		assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));                                            \
                                                                                                                   \
		struct STRUCT_NAME *obj = NULL;                                                                    \
                                                                                                                   \
		if (STRUCT_NAME##_pool_is_empty(self)) return obj;                                                 \
                                                                                                                   \
		obj = ps_list_head_first_d(&self->list, struct STRUCT_NAME);                                       \
		assert(obj);                                                                                       \
		ps_list_rem_d(obj);                                                                                \
                                                                                                                   \
		return obj;                                                                                        \
	}                                                                                                          \
                                                                                                                   \
	static inline struct STRUCT_NAME *STRUCT_NAME##_pool_remove(struct STRUCT_NAME##_pool *self)               \
	{                                                                                                          \
		assert(self != NULL);                                                                              \
		assert(self->use_lock);                                                                            \
                                                                                                                   \
		struct STRUCT_NAME *obj      = NULL;                                                               \
		bool                is_empty = STRUCT_NAME##_pool_is_empty(self);                                  \
		if (is_empty) return obj;                                                                          \
                                                                                                                   \
		LOCK_LOCK(&self->lock);                                                                            \
		obj = STRUCT_NAME##_pool_remove_nolock(self);                                                      \
		LOCK_UNLOCK(&self->lock);                                                                          \
		return obj;                                                                                        \
	}                                                                                                          \
                                                                                                                   \
	static inline void STRUCT_NAME##_pool_add_nolock(struct STRUCT_NAME##_pool *self, struct STRUCT_NAME *obj) \
	{                                                                                                          \
		assert(self != NULL);                                                                              \
		assert(obj != NULL);                                                                               \
		assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));                                            \
                                                                                                                   \
		ps_list_head_add_d(&self->list, obj);                                                              \
	}                                                                                                          \
                                                                                                                   \
	static inline void STRUCT_NAME##_pool_add(struct STRUCT_NAME##_pool *self, struct STRUCT_NAME *obj)        \
	{                                                                                                          \
		assert(self != NULL);                                                                              \
		assert(obj != NULL);                                                                               \
		assert(self->use_lock);                                                                            \
                                                                                                                   \
		LOCK_LOCK(&self->lock);                                                                            \
		STRUCT_NAME##_pool_add_nolock(self, obj);                                                          \
		LOCK_UNLOCK(&self->lock);                                                                          \
	}
