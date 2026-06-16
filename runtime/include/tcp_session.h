#pragma once

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <threads.h>
#include <unistd.h>

#include "debuglog.h"
#include "likely.h"
#include "panic.h"

/*
 * Bound on the non-blocking drain performed during a graceful close. 32 passes of an 8 KiB buffer
 * (256 KiB) is far larger than any already-buffered request remainder we expect to see, while still
 * guaranteeing the single listener thread cannot be held by a misbehaving client. See tcp_session_close().
 */
#define TCP_SESSION_CLOSE_DRAIN_MAX_PASSES    32
#define TCP_SESSION_CLOSE_DRAIN_BUFFER_LENGTH 8192

static inline void
tcp_session_close(int client_socket, struct sockaddr *client_address)
{
	/* Should never close 0, 1, or 2 */
	assert(client_socket != STDIN_FILENO);
	assert(client_socket != STDOUT_FILENO);
	assert(client_socket != STDERR_FILENO);

	/*
	 * Graceful close to avoid sending the client a TCP RST (issue #185).
	 *
	 * SLEdge often answers a request before it has finished reading it: the short-circuit error
	 * responses (400/404/429/500/503) are written and the socket closed while the client may still be
	 * sending its request body. A bare close() with unread data still in the kernel receive buffer makes
	 * Linux discard that data and emit a RST instead of a graceful FIN. The client's HTTP stack then
	 * reports it as "connection reset by peer" or, on a pooled keepalive connection, as the Go net/http
	 * warning "Unsolicited response received on idle HTTP channel" that issue #185 describes.
	 *
	 * We instead (1) half-close the write side so the client receives our FIN (and, having a complete
	 * response, stops sending), then (2) drain any already-buffered inbound data so close() no longer
	 * sees unread data. The drain is non-blocking and bounded, so the listener thread can never block or
	 * spin on a slow client. This removes the RST for every request the client has finished sending
	 * (all bodyless GETs and small POSTs - i.e. the reported scenario). A client that keeps streaming a
	 * large body to a rejected route may still observe its own write abort as EPIPE; fully suppressing
	 * that would require lingering on the request in the event loop, which is out of scope here.
	 */
	shutdown(client_socket, SHUT_WR);

	thread_local static char drain[TCP_SESSION_CLOSE_DRAIN_BUFFER_LENGTH];
	for (int pass = 0; pass < TCP_SESSION_CLOSE_DRAIN_MAX_PASSES; pass++) {
		ssize_t drained = read(client_socket, drain, sizeof(drain));
		if (drained == 0) break; /* EOF: client closed its side, nothing left to drain */
		if (drained < 0) {
			if (errno == EINTR) continue;
			/* EAGAIN/EWOULDBLOCK: receive buffer is empty, so close() will not RST. Any other
			 * error means the connection is already broken. Either way, stop draining. */
			break;
		}
		/* Drained a full buffer; more may be queued, so loop again (bounded). */
	}

	if (unlikely(close(client_socket) < 0)) {
		char client_address_text[INET6_ADDRSTRLEN] = {'\0'};
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
 * @returns nwritten on success, -errno, -EAGAIN on block
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
			return -EAGAIN;
		} else {
			return -errno;
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
 * @returns nwritten on success, -errno on error, -eagain on block
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
			return -EAGAIN;
		} else {
			return -errno;
		}
	}

	return received;
}
