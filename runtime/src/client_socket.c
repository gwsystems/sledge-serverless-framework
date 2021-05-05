#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client_socket.h"
#include "debuglog.h"
#include "http_response.h"
#include "http_total.h"
#include "likely.h"
#include "panic.h"
#include "worker_thread.h"

void
client_socket_close(int client_socket, struct sockaddr *client_address)
{
	/* Should never close 0, 1, or 2 */
	assert(client_socket != STDIN_FILENO);
	assert(client_socket != STDOUT_FILENO);
	assert(client_socket != STDERR_FILENO);

	if (unlikely(close(client_socket) < 0)) {
		char client_address_text[INET6_ADDRSTRLEN] = {};
		if (unlikely(inet_ntop(AF_INET, &client_address, client_address_text, INET6_ADDRSTRLEN) == NULL)) {
			debuglog("Failed to log client_address: %s", strerror(errno));
		}
		debuglog("Error closing client socket %d associated with %s - %s", client_socket, client_address_text,
		         strerror(errno));
	}
}

/**
 * Rejects request due to admission control or error
 * @param client_socket - the client we are rejecting
 * @param status_code - either 503 or 400
 */
int
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

			debuglog("Error with %s\n", strerror(errno));

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
