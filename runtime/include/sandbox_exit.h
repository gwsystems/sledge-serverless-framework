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
		 * TODO: I think this executes when running inside the sandbox, as it hasn't yet yielded
		 * See Issue #224 at https://github.com/gwsystems/sledge-serverless-framework/issues/224
		 */
		sandbox_set_as_complete(exiting_sandbox, SANDBOX_RETURNED);
		break;
	case SANDBOX_BLOCKED:
	case SANDBOX_ERROR:
		break;
	default:
		panic("Cooperatively switching from a sandbox in a non-terminal %s state\n",
		      sandbox_state_stringify(exiting_sandbox->state));
	}
}
