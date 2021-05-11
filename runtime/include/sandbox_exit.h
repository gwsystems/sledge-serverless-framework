#pragma once

#include <assert.h>
#include <stddef.h>

#include "panic.h"
#include "sandbox_state.h"
#include "sandbox_set_as_complete.h"

/**
 * Conditionally triggers appropriate state changes for exiting sandboxes
 * @param exiting_sandbox - The sandbox that ran to completion
 */
static inline void
sandbox_exit(struct sandbox *exiting_sandbox)
{
	assert(exiting_sandbox != NULL);

	switch (exiting_sandbox->state) {
	case SANDBOX_RETURNED:
		/*
		 * We draw a distinction between RETURNED and COMPLETED because a sandbox cannot add itself to the
		 * completion queue
		 */
		sandbox_set_as_complete(exiting_sandbox, SANDBOX_RETURNED);
		break;
	case SANDBOX_BLOCKED:
		/* Cooperative yield, so just break */
		break;
	case SANDBOX_ERROR:
		/* Terminal State, so just break */
		break;
	default:
		panic("Cooperatively switching from a sandbox in a non-terminal %s state\n",
		      sandbox_state_stringify(exiting_sandbox->state));
	}
}
