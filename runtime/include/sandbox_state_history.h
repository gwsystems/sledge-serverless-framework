#pragma once

#include "sandbox_state.h"
#include "sandbox_types.h"

/* TODO: Define a struct and make the first argument a struct sandbox_state_history */

static inline void
sandbox_state_history_append(struct sandbox *self, sandbox_state_t state)
{
#ifdef LOG_STATE_CHANGES
	if (likely(self->state_history_count < SANDBOX_STATE_HISTORY_CAPACITY)) {
		sandbox->state_history[self->state_history_count++] = state;
	}
#endif
}

static inline void
sandbox_state_history_init(struct sandbox *self)
{
#ifdef LOG_STATE_CHANGES
	sandbox_state_history_append(self, SANDBOX_UNINITIALIZED);
	memset(&sandbox->state_history, 0, SANDBOX_STATE_HISTORY_CAPACITY * sizeof(sandbox_state_t));
	sandbox->state_history_count                           = 0;
	sandbox->state_history[sandbox->state_history_count++] = SANDBOX_UNINITIALIZED;
#endif
}
