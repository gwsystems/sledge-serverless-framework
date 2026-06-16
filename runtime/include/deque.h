/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */
#ifndef DEQUE_H
#define DEQUE_H

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

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
 *
 * The backing buffer is heap-allocated at init to the requested capacity (rounded up to a power of two)
 * and indexed circularly: the top/bottom counters grow monotonically and are masked into the buffer. The
 * occupancy cap (size - 1) keeps the producer from lapping the consumers, so the masked indices of live
 * elements never collide. Memory therefore scales with the configured capacity rather than a fixed
 * compile-time maximum.
 *
 * Growing the buffer when full is intentionally not implemented: the deque is consumed lock-free by
 * stealers concurrently with the producer, so reallocating (and freeing) the backing buffer underneath
 * an in-flight steal would be a use-after-free. A safe grow would require synchronizing every stealer or
 * a lock-free resizable variant; until that is designed and perf-tested the deque returns -ENOSPC when
 * full and the caller applies backpressure.
 */

/* Upper bound on a deque's capacity. A sanity guard on the requested size, not an allocation size. */
#define DEQUE_MAX_SZ (1 << 23)

static inline size_t
deque_round_up_to_pow2(size_t v)
{
	size_t p = 1;
	while (p < v) p <<= 1;
	return p;
}

#define DEQUE_PROTOTYPE(name, type)                                                                 \
	struct deque_##name {                                                                       \
		type *wrk;                                                                          \
		long  size;                                                                         \
		long  mask;                                                                         \
                                                                                                    \
		volatile long top;                                                                  \
		volatile long bottom;                                                               \
	};                                                                                          \
                                                                                                    \
	/* Allocates the backing buffer. Returns 0 on success, -ENOMEM on allocation failure. */    \
	static inline int deque_init_##name(struct deque_##name *q, size_t sz)                      \
	{                                                                                           \
		if (sz == 0) sz = DEQUE_MAX_SZ;                                                     \
		assert(sz <= DEQUE_MAX_SZ);                                                         \
		sz = deque_round_up_to_pow2(sz);                                                    \
                                                                                                    \
		q->wrk = (type *)calloc(sz, sizeof(type));                                          \
		if (q->wrk == NULL) return -ENOMEM;                                                 \
                                                                                                    \
		q->size   = (long)sz;                                                               \
		q->mask   = (long)sz - 1;                                                           \
		q->top    = 0;                                                                      \
		q->bottom = 0;                                                                      \
                                                                                                    \
		return 0;                                                                           \
	}                                                                                           \
                                                                                                    \
	static inline void deque_free_##name(struct deque_##name *q)                                \
	{                                                                                           \
		free(q->wrk);                                                                       \
		q->wrk = NULL;                                                                      \
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
		/* Bounded by the configured capacity; caller applies backpressure on -ENOSPC. */   \
		if (q->size - 1 < (cb - ct)) return -ENOSPC;                                        \
                                                                                                    \
		q->wrk[cb & q->mask] = *w;                                                          \
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
		*w = q->wrk[cb & q->mask];                                                          \
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
		*w = deque->wrk[ct & deque->mask];                                                  \
		if (__sync_bool_compare_and_swap(&deque->top, ct, ct + 1) == false) return -EAGAIN; \
                                                                                                    \
		return 0;                                                                           \
	}

#endif /* DEQUE_H */
