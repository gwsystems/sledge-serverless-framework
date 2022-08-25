#pragma once

#include <assert.h>
#include <spinlock/mcs.h>
#include <stdint.h>

#include "arch/getcycles.h"
#include "runtime.h"


/* A linked list of nodes */
struct lock_wrapper {
	uint64_t          longest_held;
	uint64_t          total_held;
	ck_spinlock_mcs_t lock;
};

/* A node on the linked list */
struct lock_node {
	struct ck_spinlock_mcs node;
	uint64_t               time_locked;
};

typedef struct lock_wrapper lock_t;
typedef struct lock_node    lock_node_t;

/**
 * Initializes a lock
 * @param lock - the address of the lock
 */
static inline void
lock_init(lock_t *self)
{
	self->total_held   = 0;
	self->longest_held = 0;
	ck_spinlock_mcs_init(&self->lock);
}

/**
 * Checks if a lock is locked
 * @param lock - the address of the lock
 * @returns bool if lock is locked
 */
static inline bool
lock_is_locked(lock_t *self)
{
	return ck_spinlock_mcs_locked(&self->lock);
}

/**
 * Locks a lock, keeping track of overhead
 * @param lock - the address of the lock
 * @param node - node to add to lock
 */
static inline void
lock_lock(lock_t *self, lock_node_t *node)
{
	assert(node->time_locked == 0);

	node->time_locked = __getcycles();
	ck_spinlock_mcs_lock(&self->lock, &node->node);
}

/**
 * Unlocks a lock
 * @param lock - the address of the lock
 * @param node - node used when calling lock_lock
 */
static inline void
lock_unlock(lock_t *self, lock_node_t *node)
{
	assert(node->time_locked > 0);

	ck_spinlock_mcs_unlock(&self->lock, &node->node);
	uint64_t now = __getcycles();
	assert(node->time_locked < now);
	uint64_t duration = now - node->time_locked;
	node->time_locked = 0;
	if (unlikely(duration > self->longest_held)) { self->longest_held = duration; }
	self->total_held += duration;
}
