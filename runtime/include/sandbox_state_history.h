#pragma once

#include "sandbox_state.h"
#include "sandbox_types.h"

#ifdef LOG_STATE_CHANGES
#define SANDBOX_STATE_HISTORY_CAPACITY 100
#else
#define SANDBOX_STATE_HISTORY_CAPACITY 0
#endif

struct sandbox_state_history {
	uint16_t        size;
	sandbox_state_t buffer[SANDBOX_STATE_HISTORY_CAPACITY];
};

static inline void
sandbox_state_history_init(struct sandbox_state_history *self)
{
#ifdef LOG_STATE_CHANGES
	memset(self, 0,
	       sizeof(struct sandbox_state_history) + SANDBOX_STATE_HISTORY_CAPACITY * sizeof(sandbox_state_t));
#endif
}

static inline void
sandbox_state_history_append(struct sandbox_state_history *self, sandbox_state_t state)
{
#ifdef LOG_STATE_CHANGES
	if (likely(self->size < SANDBOX_STATE_HISTORY_CAPACITY)) { self->buffer[self->size++] = state; }
#endif
}
