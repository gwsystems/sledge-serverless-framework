#include "sandbox_state_transition.h"

void
sandbox_state_transition_hook_default(struct sandbox *sandbox)
{
	return;
}

/* Called while nonpreemptable */
sandbox_state_transition_hook_t sandbox_state_transition_to_hooks[SANDBOX_STATE_COUNT] = {

  [SANDBOX_UNINITIALIZED] = sandbox_state_transition_hook_default,
  [SANDBOX_ALLOCATED]     = sandbox_state_transition_hook_default,
  [SANDBOX_INITIALIZED]   = sandbox_state_transition_hook_default,
  [SANDBOX_RUNNABLE]      = sandbox_state_transition_hook_default,
  [SANDBOX_INTERRUPTED]   = sandbox_state_transition_hook_default,
  [SANDBOX_PREEMPTED]     = sandbox_state_transition_hook_default,
  [SANDBOX_RUNNING_SYS]   = sandbox_state_transition_hook_default,
  [SANDBOX_RUNNING_USER]  = sandbox_state_transition_hook_default,
  [SANDBOX_ASLEEP]        = sandbox_state_transition_hook_default,
  [SANDBOX_RETURNED]      = sandbox_state_transition_hook_default,
  [SANDBOX_COMPLETE]      = sandbox_state_transition_hook_default,
  [SANDBOX_ERROR]         = sandbox_state_transition_hook_default};

/* Called while nonpreemptable */
sandbox_state_transition_hook_t sandbox_state_transition_from_hooks[SANDBOX_STATE_COUNT] = {

  [SANDBOX_UNINITIALIZED] = sandbox_state_transition_hook_default,
  [SANDBOX_ALLOCATED]     = sandbox_state_transition_hook_default,
  [SANDBOX_INITIALIZED]   = sandbox_state_transition_hook_default,
  [SANDBOX_RUNNABLE]      = sandbox_state_transition_hook_default,
  [SANDBOX_INTERRUPTED]   = sandbox_state_transition_hook_default,
  [SANDBOX_PREEMPTED]     = sandbox_state_transition_hook_default,
  [SANDBOX_RUNNING_SYS]   = sandbox_state_transition_hook_default,
  [SANDBOX_RUNNING_USER]  = sandbox_state_transition_hook_default,
  [SANDBOX_ASLEEP]        = sandbox_state_transition_hook_default,
  [SANDBOX_RETURNED]      = sandbox_state_transition_hook_default,
  [SANDBOX_COMPLETE]      = sandbox_state_transition_hook_default,
  [SANDBOX_ERROR]         = sandbox_state_transition_hook_default};
