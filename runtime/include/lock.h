#pragma once

#include <spinlock/mcs.h>

#include "runtime.h"
#include "software_interrupt.h"

typedef ck_spinlock_mcs_t lock_t;

/**
 * Initializes a lock of type lock_t
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
 * @param unique_variable_name - a unique prefix to hygienically namespace an associated lock/unlock pair
 */
#define LOCK_LOCK_WITH_BOOKKEEPING(lock, unique_variable_name)                        \
	assert(!runtime_is_worker() || !software_interrupt_is_enabled());             \
	struct ck_spinlock_mcs _hygiene_##unique_variable_name##_node;                \
	uint64_t               _hygiene_##unique_variable_name##_pre = __getcycles(); \
	ck_spinlock_mcs_lock((lock), &(_hygiene_##unique_variable_name##_node));      \
	worker_thread_lock_duration += (__getcycles() - _hygiene_##unique_variable_name##_pre);

/**
 * Unlocks a lock
 * @param lock - the address of the lock
 * @param unique_variable_name - a unique prefix to hygienically namespace an associated lock/unlock pair
 */
#define LOCK_UNLOCK_WITH_BOOKKEEPING(lock, unique_variable_name)          \
	assert(!runtime_is_worker() || !software_interrupt_is_enabled()); \
	ck_spinlock_mcs_unlock(lock, &(_hygiene_##unique_variable_name##_node));

/**
 * Locks a lock, keeping track of overhead
 * Assumes the availability of DEFAULT as a hygienic prefix for DEFAULT_node and DEFAULT_pre
 *
 * As such, this API can only be used once in a lexical scope.
 *
 * Use LOCK_LOCK_WITH_BOOKKEEPING and LOCK_UNLOCK_WITH_BOOKKEEPING if multiple locks are required
 * @param lock - the address of the lock
 */
#define LOCK_LOCK(lock) LOCK_LOCK_WITH_BOOKKEEPING(lock, DEFAULT)

/**
 * Unlocks a lock
 * Uses lock node NODE_DEFAULT and timestamp PRE_DEFAULT, so this assumes use of LOCK_LOCK
 * This API can only be used once in a lexical scope. If this isn't true, use LOCK_LOCK_WITH_BOOKKEEPING and
 * LOCK_UNLOCK_WITH_BOOKKEEPING
 * @param lock - the address of the lock
 */
#define LOCK_UNLOCK(lock) LOCK_UNLOCK_WITH_BOOKKEEPING(lock, DEFAULT)
