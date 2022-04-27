#pragma once

#include "current_sandbox.h"
#include "sandbox_types.h"

/**
 * Receive and Parse the Request for the current sandbox
 * @return 0 if message parsing complete, -1 on error, -2 if buffers run out of space
 */
static inline int
sandbox_receive_request(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	struct http_session *session = sandbox->http;

	return http_session_receive_request(session, current_sandbox_sleep);
}
