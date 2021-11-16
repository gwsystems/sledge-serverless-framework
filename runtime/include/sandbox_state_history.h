#pragma once

#include "sandbox_state.h"
#include "sandbox_types.h"

static inline void
sandbox_state_history_append(struct sandbox *sandbox, sandbox_state_t state)
{
#ifdef LOG_STATE_CHANGES
	if (likely(sandbox->state_history_count < SANDBOX_STATE_HISTORY_CAPACITY)) {
		sandbox->state_history[sandbox->state_history_count++] = state;
	}
#endif
}
