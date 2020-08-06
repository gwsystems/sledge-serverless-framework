#pragma once

#include <spinlock/mcs.h>

#include "runtime.h"

#define LOCK_T ck_spinlock_mcs_t

/**
 * Initializes a lock of type LOCK_T
 * @param lock - the address of the lock
 */
#define LOCK_INIT(lock) ck_spinlock_mcs_init((lock))

/**
 * Checks if a lock is locked
 * @param lock - the address of the lock
 * @returns bool if lock is locked
 */

#define LOCK_IS_LOCKED(lock) ck_spinlock_mcs_locked((lock))

/**
 * Locks a lock, keeping track of overhead
 * @param lock - the address of the lock
 * @param node_name - a unique name to identify the lock node, which is prefixed by NODE_
 */
#define LOCK_LOCK_VERBOSE(lock, node_name)                 \
	struct ck_spinlock_mcs(NODE_##node_name);          \
	uint64_t PRE_##node_name = __getcycles();          \
	ck_spinlock_mcs_lock((lock), &(NODE_##node_name)); \
	worker_thread_lock_duration += (__getcycles() - PRE_##node_name)

/**
 * Unlocks a lock
 * @param lock - the address of the lock
 * @param node_name - a unique name to identify the lock node, which is prefixed by NODE_
 */
#define LOCK_UNLOCK_VERBOSE(lock, node_name) ck_spinlock_mcs_unlock(lock, &(NODE_##node_name))

/**
 * Locks a lock, keeping track of overhead
 * Automatically assigns a lock node NODE_DEFAULT and a timestamp PRE_DEFAULT
 * This API can only be used once in a lexical scope. If this isn't true, use LOCK_LOCK_VERBOSE and LOCK_UNLOCK_VERBOSE
 * @param lock - the address of the lock
 */
#define LOCK_LOCK(lock) LOCK_LOCK_VERBOSE(lock, DEFAULT)

/**
 * Unlocks a lock
 * Uses lock node NODE_DEFAULT and timestamp PRE_DEFAULT, so this assumes use of LOCK_LOCK
 * This API can only be used once in a lexical scope. If this isn't true, use LOCK_LOCK_VERBOSE and LOCK_UNLOCK_VERBOSE
 * @param lock - the address of the lock
 */
#define LOCK_UNLOCK(lock) LOCK_UNLOCK_VERBOSE(lock, DEFAULT)
