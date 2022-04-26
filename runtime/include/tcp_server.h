#pragma once

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "debuglog.h"
#include "likely.h"

/*
 * Defines the listen backlog, the queue length for completely established socketeds waiting to be accepted
 * If this value is greater than the value in /proc/sys/net/core/somaxconn (typically 128), then it is silently
 * truncated to this value. See man listen(2) for info
 *
 * When configuring the number of sockets to handle, the queue length of incomplete sockets defined in
 * /proc/sys/net/ipv4/tcp_max_syn_backlog should also be considered. Optionally, enabling syncookies removes this
 * maximum logical length. See tcp(7) for more info.
 */
#define TCP_SERVER_MAX_PENDING_CLIENT_REQUESTS 128
#if TCP_SERVER_MAX_PENDING_CLIENT_REQUESTS > 128
#warning \
  "TCP_SERVER_MAX_PENDING_CLIENT_REQUESTS likely exceeds the value in /proc/sys/net/core/somaxconn and thus may be silently truncated";
#endif

/* L4 TCP State */
struct tcp_server {
	uint16_t           port;
	struct sockaddr_in socket_address;
	int                socket_descriptor;
};

static inline void
tcp_server_init(struct tcp_server *server, uint16_t port)
{
	server->port              = port;
	server->socket_descriptor = -1;
}

/**
 * Start the module as a server listening at module->port
 * @param module
 * @returns 0 on success, -1 on error
 */
static inline int
tcp_server_listen(struct tcp_server *server)
{
	int rc;
	int optval = 1;

	/* Allocate a new TCP/IP socket, setting it to be non-blocking */
	int socket_descriptor = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (unlikely(socket_descriptor < 0)) goto err_create_socket;

	/* Socket should never have returned on fd 0, 1, or 2 */
	assert(socket_descriptor != STDIN_FILENO);
	assert(socket_descriptor != STDOUT_FILENO);
	assert(socket_descriptor != STDERR_FILENO);

	/* Configure the socket to allow multiple sockets to bind to the same host and port */
	rc = setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	if (unlikely(rc < 0)) goto err_set_socket_option;
	optval = 1;
	rc     = setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (unlikely(rc < 0)) goto err_set_socket_option;

	/* Bind name [all addresses]:[module->port] to socket */
	server->socket_descriptor              = socket_descriptor;
	server->socket_address.sin_family      = AF_INET;
	server->socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server->socket_address.sin_port        = htons((unsigned short)server->port);
	rc = bind(socket_descriptor, (struct sockaddr *)&server->socket_address, sizeof(server->socket_address));
	if (unlikely(rc < 0)) goto err_bind_socket;

	/* Listen to the interface */
	rc = listen(socket_descriptor, TCP_SERVER_MAX_PENDING_CLIENT_REQUESTS);
	if (unlikely(rc < 0)) goto err_listen;

	printf("Listening on port %d\n", server->port);

	rc = 0;
done:
	return rc;
err_listen:
err_bind_socket:
	server->socket_descriptor = -1;
err_set_socket_option:
	close(socket_descriptor);
err_create_socket:
err:
	debuglog("Socket Error: %s", strerror(errno));
	rc = -1;
	goto done;
}

static inline int
tcp_server_close(struct tcp_server *server)
{
	return close(server->socket_descriptor);
}
