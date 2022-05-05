#pragma once

#include <assert.h>

#include "sandbox_state.h"
#include "sandbox_types.h"


typedef void (*sandbox_state_transition_hook_t)(struct sandbox *);
extern sandbox_state_transition_hook_t sandbox_state_transition_from_hooks[SANDBOX_STATE_COUNT];
extern sandbox_state_transition_hook_t sandbox_state_transition_to_hooks[SANDBOX_STATE_COUNT];

static inline void
sandbox_state_transition_from_hook(struct sandbox *sandbox, sandbox_state_t state)
{
	assert(sandbox != NULL);
	assert(state < SANDBOX_STATE_COUNT);

	sandbox_state_transition_from_hooks[state](sandbox);
}

static inline void
sandbox_state_transition_to_hook(struct sandbox *sandbox, sandbox_state_t state)
{
	assert(sandbox != NULL);
	assert(state < SANDBOX_STATE_COUNT);

	sandbox_state_transition_to_hooks[state](sandbox);
}

static inline void
sandbox_state_transition_from_hook_register(sandbox_state_t state, sandbox_state_transition_hook_t cb)
{
	assert(state < SANDBOX_STATE_COUNT);
	assert(cb != NULL);

	sandbox_state_transition_from_hooks[state] = cb;
}

static inline void
sandbox_state_transition_to_hook_register(sandbox_state_t state, sandbox_state_transition_hook_t cb)
{
	assert(state < SANDBOX_STATE_COUNT);
	assert(cb != NULL);

	sandbox_state_transition_to_hooks[state] = cb;
}
