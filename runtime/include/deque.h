/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */
#ifndef DEQUE_H
#define DEQUE_H

/*
 * This was implemented by referring to:
 * https://github.com/cpp-taskflow/cpp-taskflow/blob/9c28ccec910346a9937c40db7bdb542262053f9c/taskflow/executor/workstealing.hpp
 *
 * which is based on the following papers:
 *
 * The work stealing queue described in the paper, "Dynamic Circular Work-stealing Deque," SPAA, 2005.
 * Only the queue owner can perform pop and push operations, while others can steal data from the queue.
 *
 * PPoPP implementation paper, "Correct and Efficient Work-Stealing for Weak Memory Models"
 * https://www.di.ens.fr/~zappa/readings/ppopp13.pdf
 */

/* TODO: Implement the ability to dynamically resize! Issue #89 */
#define DEQUE_MAX_SZ (1 << 23)

#define DEQUE_PROTOTYPE(name, type)                                                                 \
	struct deque_##name {                                                                       \
		type wrk[DEQUE_MAX_SZ];                                                             \
		long size;                                                                          \
                                                                                                    \
		volatile long top;                                                                  \
		volatile long bottom;                                                               \
	};                                                                                          \
                                                                                                    \
	static inline void deque_init_##name(struct deque_##name *q, size_t sz)                     \
	{                                                                                           \
		memset(q, 0, sizeof(struct deque_##name));                                          \
                                                                                                    \
		if (sz) {                                                                           \
			/* only for size with pow of 2 */                                           \
			/* assert((sz & (sz - 1)) == 0); */                                         \
			assert(sz <= DEQUE_MAX_SZ);                                                 \
		} else {                                                                            \
			sz = DEQUE_MAX_SZ;                                                          \
		}                                                                                   \
                                                                                                    \
		q->size = sz;                                                                       \
	}                                                                                           \
                                                                                                    \
	/* Use mutual exclusion locks around push/pop if multi-threaded. */                         \
	static inline int deque_push_##name(struct deque_##name *q, type *w)                        \
	{                                                                                           \
		long ct, cb;                                                                        \
                                                                                                    \
		ct = q->top;                                                                        \
		cb = q->bottom;                                                                     \
                                                                                                    \
		/* nope, fixed size only */                                                         \
		if (q->size - 1 < (cb - ct)) return -ENOSPC;                                        \
                                                                                                    \
		q->wrk[cb] = *w;                                                                    \
		__sync_synchronize();                                                               \
		if (__sync_bool_compare_and_swap(&q->bottom, cb, cb + 1) == false) assert(0);       \
                                                                                                    \
		return 0;                                                                           \
	}                                                                                           \
                                                                                                    \
	/* Use mutual exclusion locks around push/pop if multi-threaded. */                         \
	static inline int deque_pop_##name(struct deque_##name *q, type *w)                         \
	{                                                                                           \
		long ct = 0, sz = 0;                                                                \
		long cb  = q->bottom - 1;                                                           \
		int  ret = 0;                                                                       \
                                                                                                    \
		if (__sync_bool_compare_and_swap(&q->bottom, cb + 1, cb) == false) assert(0);       \
                                                                                                    \
		ct = q->top;                                                                        \
		sz = cb - ct;                                                                       \
		if (sz < 0) {                                                                       \
			if (__sync_bool_compare_and_swap(&q->bottom, cb, ct) == false) assert(0);   \
                                                                                                    \
			return -ENOENT;                                                             \
		}                                                                                   \
                                                                                                    \
		*w = q->wrk[cb];                                                                    \
		if (sz > 0) return 0;                                                               \
                                                                                                    \
		ret = __sync_bool_compare_and_swap(&q->top, ct, ct + 1);                            \
		if (__sync_bool_compare_and_swap(&q->bottom, cb, ct + 1) == false) assert(0);       \
		if (ret == false) {                                                                 \
			*w = NULL;                                                                  \
			return -ENOENT;                                                             \
		}                                                                                   \
                                                                                                    \
		return 0;                                                                           \
	}                                                                                           \
	/**                                                                                         \
	 * deque_steal                                                                              \
	 * @param deque                                                                             \
	 * @param w pointer to location to copy stolen type to                                      \
	 * @returns 0 if successful, -2 if empty, -11 if unable to perform atomic operation         \
	 */                                                                                         \
	static inline int deque_steal_##name(struct deque_##name *deque, type *w)                   \
	{                                                                                           \
		long ct, cb;                                                                        \
                                                                                                    \
		ct = deque->top;                                                                    \
		cb = deque->bottom;                                                                 \
                                                                                                    \
		/* Empty */                                                                         \
		if (ct >= cb) return -ENOENT;                                                       \
                                                                                                    \
		*w = deque->wrk[ct];                                                                \
		if (__sync_bool_compare_and_swap(&deque->top, ct, ct + 1) == false) return -EAGAIN; \
                                                                                                    \
		return 0;                                                                           \
	}

#endif /* DEQUE_H */
