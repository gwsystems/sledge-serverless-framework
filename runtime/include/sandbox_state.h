#pragma once

#include <stdatomic.h>

#include "debuglog.h"
#include "likely.h"
#include "panic.h"

typedef enum
{
	SANDBOX_UNINITIALIZED = 0, /* Assumption: mmap zeros out structure */
	SANDBOX_ALLOCATED,
	SANDBOX_INITIALIZED,
	SANDBOX_RUNNABLE,
	SANDBOX_PREEMPTED,
	SANDBOX_RUNNING_SYS,
	SANDBOX_RUNNING_USER,
	SANDBOX_ASLEEP,
	SANDBOX_RETURNED,
	SANDBOX_COMPLETE,
	SANDBOX_ERROR,
	SANDBOX_STATE_COUNT
} sandbox_state_t;

extern const char *sandbox_state_labels[SANDBOX_STATE_COUNT];

static inline const char *
sandbox_state_stringify(sandbox_state_t state)
{
	if (unlikely(state >= SANDBOX_STATE_COUNT)) panic("%d is an unrecognized sandbox state\n", state);
	return sandbox_state_labels[state];
}

#ifdef LOG_SANDBOX_COUNT
extern _Atomic uint32_t sandbox_state_count[SANDBOX_STATE_COUNT];
#endif

static inline void
sandbox_count_initialize()
{
#ifdef LOG_SANDBOX_COUNT
	for (int i = 0; i < SANDBOX_STATE_COUNT; i++) atomic_init(&sandbox_state_count[i], 0);
#endif
}

static inline void
runtime_sandbox_total_increment(sandbox_state_t state)
{
#ifdef LOG_SANDBOX_COUNT
	atomic_fetch_add(&sandbox_state_count[state], 1);
#endif
}

static inline void
runtime_sandbox_total_decrement(sandbox_state_t state)
{
#ifdef LOG_SANDBOX_COUNT
	if (atomic_load(&sandbox_state_count[state]) == 0) panic("Underflow of %s\n", sandbox_state_stringify(state));
	atomic_fetch_sub(&sandbox_state_count[state], 1);
#endif
}
