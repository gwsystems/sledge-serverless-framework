#pragma once

#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "debuglog.h"
#include "http.h"
#include "http_total.h"
#include "panic.h"
#include "likely.h"


static inline void
client_socket_close(int client_socket, struct sockaddr *client_address)
{
	/* Should never close 0, 1, or 2 */
	assert(client_socket != STDIN_FILENO);
	assert(client_socket != STDOUT_FILENO);
	assert(client_socket != STDERR_FILENO);

	if (unlikely(close(client_socket) < 0)) {
		char client_address_text[INET6_ADDRSTRLEN] = { '\0' };
		if (unlikely(inet_ntop(AF_INET, &client_address, client_address_text, INET6_ADDRSTRLEN) == NULL)) {
			debuglog("Failed to log client_address: %s", strerror(errno));
		}
		debuglog("Error closing client socket %d associated with %s - %s", client_socket, client_address_text,
		         strerror(errno));
	}
}

typedef void (*void_cb)(void);

/**
 * Writes buffer to the client socket
 * @param client_socket - the client we are rejecting
 * @param buffer - buffer to write to socket
 * @param on_eagain - cb to execute when client socket returns EAGAIN. If NULL, error out
 * @returns 0
 */
static inline int
client_socket_send(int client_socket, const char *buffer, size_t buffer_len, void_cb on_eagain)
{
	int rc;

	size_t cursor = 0;

	while (cursor < buffer_len) {
		ssize_t sent = write(client_socket, &buffer[cursor], buffer_len - cursor);
		if (sent < 0) {
			if (errno == EAGAIN) {
				if (on_eagain == NULL) {
					rc = -1;
					goto done;
				}
				on_eagain();
			} else {
				debuglog("Error sending to client: %s", strerror(errno));
				rc = -1;
				goto done;
			}
		}

		assert(sent > 0);
		cursor += (size_t)sent;
	};

	rc = 0;
done:
	return rc;
}

/**
 * Rejects request due to admission control or error
 * @param client_socket - the client we are rejecting
 * @param buffer - buffer to write to socket
 * @returns 0
 */
static inline int
client_socket_send_oneshot(int client_socket, const char *buffer, size_t buffer_len)
{
	return client_socket_send(client_socket, buffer, buffer_len, NULL);
}
