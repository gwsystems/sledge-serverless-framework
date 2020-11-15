#pragma once

#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "panic.h"
#include "debuglog.h"
#include "http_response.h"
#include "http_total.h"
#include "runtime.h"
#include "worker_thread.h"


static inline void
client_socket_close(int client_socket)
{
	if (close(client_socket) < 0) debuglog("Error closing client socket - %s", strerror(errno));
}


/**
 * Rejects request due to admission control or error
 * @param client_socket - the client we are rejecting
 * @param status_code - either 503 or 400
 */
static inline int
client_socket_send(int client_socket, int status_code)
{
	const char *response;
	int         rc;
	switch (status_code) {
	case 503:
		response = HTTP_RESPONSE_503_SERVICE_UNAVAILABLE;
		http_total_increment_5XX();
		break;
	case 400:
		response = HTTP_RESPONSE_400_BAD_REQUEST;
		http_total_increment_4XX();
		break;
	default:
		panic("%d is not a valid status code\n", status_code);
	}

	int sent    = 0;
	int to_send = strlen(response);

	while (sent < to_send) {
		rc = write(client_socket, &response[sent], to_send - sent);
		if (rc < 0) {
			if (errno == EAGAIN) { debuglog("Unexpectedly blocking on write of %s\n", response); }

			goto send_err;
		}
		sent += rc;
	};

	rc = 0;
done:
	return rc;
send_err:
	debuglog("Error sending to client: %s", strerror(errno));
	rc = -1;
	goto done;
}
