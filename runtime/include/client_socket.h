#pragma once

#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "panic.h"
#include "debuglog.h"
#include "http_response.h"
#include "runtime.h"
#include "worker_thread.h"


static inline void
client_socket_close(int client_socket)
{
	int rc = epoll_ctl(worker_thread_epoll_file_descriptor, EPOLL_CTL_DEL, client_socket, NULL);
	if (unlikely(rc < 0)) panic_err();

	if (close(client_socket) < 0) debuglog("Error closing client socket - %s", strerror(errno));
}


/**
 * Rejects request due to admission control or error
 * @param client_socket - the client we are rejecting
 * @param status_code - either 503 or 400
 */
static inline void
client_socket_send(int client_socket, int status_code)
{
	const char *response;
	switch (status_code) {
	case 503:
		response = HTTP_RESPONSE_503_SERVICE_UNAVAILABLE;
		atomic_fetch_add(&runtime_total_5XX_responses, 1);
		break;
	case 400:
		response = HTTP_RESPONSE_400_BAD_REQUEST;
		break;
	default:
		panic("%d is not a valid status code\n", status_code);
	}

	int rc;
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

done:
	return;
send_err:
	debuglog("Error sending to client: %s", strerror(errno));
	goto done;
}
