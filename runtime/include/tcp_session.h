#pragma once

#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "debuglog.h"
#include "panic.h"
#include "likely.h"

static inline void
tcp_session_close(int client_socket, struct sockaddr *client_address)
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

typedef void (*void_star_cb)(void *);

/**
 * Writes buffer to the client socket
 * @param client_socket - the client
 * @param buffer - buffer to write to socket
 * @param on_eagain - cb to execute when client socket returns EAGAIN. If NULL, error out
 * @returns nwritten on success, -1 on error, -2 unused, -3 on eagain
 */
static inline ssize_t
tcp_session_send(int client_socket, const char *buffer, size_t buffer_len, void_star_cb on_eagain, void *dataptr)
{
	assert(buffer != NULL);
	assert(buffer_len > 0);

	ssize_t sent = write(client_socket, buffer, buffer_len);
	if (sent < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			if (on_eagain != NULL) on_eagain(dataptr);
			return -3;
		} else {
			return -1;
		}
	}

	return sent;
}

/**
 * Writes buffer to the client socket
 * @param client_socket - the client
 * @param buffer - buffer to reach the socket into
 * @param buffer_len - buffer to reach the socket into
 * @param on_eagain - cb to execute when client socket returns EAGAIN. If NULL, error out
 * @returns nwritten on success, -1 on error, -2 unused, -3 on eagain
 */
static inline ssize_t
tcp_session_recv(int client_socket, char *buffer, size_t buffer_len, void_star_cb on_eagain, void *dataptr)
{
	assert(buffer != NULL);
	assert(buffer_len > 0);

	ssize_t received = read(client_socket, buffer, buffer_len);
	if (received < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			if (on_eagain != NULL) on_eagain(dataptr);
			return -3;
		} else {
			return -1;
		}
	}

	return received;
}
