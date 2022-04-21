#pragma once

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client_socket.h"
#include "debuglog.h"
#include "http_request.h"
#include "http_parser.h"
#include "http_parser_settings.h"
#include "vec.h"

#define u8 uint8_t
VEC(u8)

struct http_session {
	/* HTTP State */
	struct sockaddr     client_address; /* client requesting connection! */
	int                 socket;
	http_parser         http_parser;
	struct http_request http_request;
	struct vec_u8       request;
	struct vec_u8       response;
};

/**
 * @param session sandbox that we want to init
 * @returns 0 on success, -1 on error
 */
static inline int
http_session_init(struct http_session *session, size_t max_request_size, size_t max_response_size,
                  int socket_descriptor, const struct sockaddr *socket_address)
{
	assert(session != NULL);
	assert(socket_address != NULL);

	session->socket = socket_descriptor;
	memcpy(&session->client_address, socket_address, sizeof(struct sockaddr));

	http_parser_init(&session->http_parser, HTTP_REQUEST);

	/* Set the session as the data the http-parser has access to */
	session->http_parser.data = &session->http_request;

	int rc;
	rc = vec_u8_init(&session->request, max_request_size);
	if (rc < 0) return -1;

	rc = vec_u8_init(&session->response, max_response_size);
	if (rc < 0) {
		vec_u8_deinit(&session->request);
		return -1;
	}

	return 0;
}

static inline struct http_session *
http_session_alloc(size_t max_request_size, size_t max_response_size, int socket_descriptor,
                   const struct sockaddr *socket_address)
{
	struct http_session *session = calloc(sizeof(struct http_session), 1);
	if (session == NULL) return NULL;

	int rc = http_session_init(session, max_request_size, max_response_size, socket_descriptor, socket_address);
	if (rc != 0) {
		free(session);
		return NULL;
	}

	return session;
}

/**
 * Deinitialize Linear Memory, cleaning up the backing buffer
 * @param sandbox
 */
static inline void
http_session_deinit(struct http_session *session)
{
	assert(session);
	vec_u8_deinit(&session->request);
	vec_u8_deinit(&session->response);
}

static inline void
http_session_free(struct http_session *session)
{
	assert(session);
	http_session_deinit(session);
	free(session);
}

/**
 * Writes buffer to the client socket
 * @param session - the HTTP session we want to send a 500 to
 * @param on_eagain - cb to execute when client socket returns EAGAIN. If NULL, error out
 * @returns 0 on success, -1 on error.
 */
static inline int
http_session_send_err(struct http_session *session, int status_code, void_cb on_eagain)
{
	return client_socket_send(session->socket, http_header_build(status_code), http_header_len(status_code),
	                          on_eagain);
}

static inline int
http_session_send_err_oneshot(struct http_session *session, int status_code)
{
	return client_socket_send_oneshot(session->socket, http_header_build(status_code),
	                                  http_header_len(status_code));
}

static inline int
http_session_send_response(struct http_session *session, const char *response_content_type, void_cb on_eagain)
{
	struct vec_u8 *response = &session->response;
	assert(response != NULL);

	int rc;

	/* Determine values to template into our HTTP response */
	const char *content_type = strlen(response_content_type) > 0 ? response_content_type : "text/plain";

	/* Send HTTP Response Header and Body */
	rc = http_header_200_write(session->socket, content_type, response->length);
	if (rc < 0) goto err;

	rc = client_socket_send(session->socket, (const char *)response->buffer, response->length, on_eagain);
	if (rc < 0) goto err;

	http_total_increment_2xx();
	rc = 0;

done:
	return rc;
err:
	debuglog("Error sending to client: %s", strerror(errno));
	rc = -1;
	goto done;
}

static inline void
http_session_close(struct http_session *session)
{
	return client_socket_close(session->socket, &session->client_address);
}


/**
 * Receive and Parse the Request for the current sandbox
 * @return 0 if message parsing complete, -1 on error, -2 if buffers run out of space
 */
static inline int
http_session_receive(struct http_session *session, void_cb on_eagain)
{
	assert(session != NULL);

	int rc = 0;

	struct vec_u8 *request = &session->request;
	assert(request->capacity > 0);
	assert(request->length < request->capacity);

	while (!session->http_request.message_end) {
		/* Read from the Socket */

		/* Structured to closely follow usage example at https://github.com/nodejs/http-parser */
		http_parser                *parser   = &session->http_parser;
		const http_parser_settings *settings = http_parser_settings_get();

		size_t request_length   = request->length;
		size_t request_capacity = request->capacity;

		if (request_length >= request_capacity) {
			debuglog("Ran out of Request Buffer before message end\n");
			goto err_nobufs;
		}

		ssize_t bytes_received = recv(session->socket, &request->buffer[request_length],
		                              request_capacity - request_length, 0);

		if (bytes_received < 0) {
			if (errno == EAGAIN) {
				if (on_eagain == NULL) {
					goto err_eagain;
				} else {
					on_eagain();
					continue;
				}
			} else {
				debuglog("Error reading socket %d - %s\n", session->socket, strerror(errno));
				goto err;
			}
		}

		/* If we received an EOF before we were able to parse a complete HTTP header, request is malformed */
		if (bytes_received == 0 && !session->http_request.message_end) {
			char client_address_text[INET6_ADDRSTRLEN] = {};
			if (unlikely(inet_ntop(AF_INET, &session->client_address, client_address_text, INET6_ADDRSTRLEN)
			             == NULL)) {
				debuglog("Failed to log client_address: %s", strerror(errno));
			}

			debuglog("recv returned 0 before a complete request was received: socket: %d. Address: %s\n",
			         session->socket, client_address_text);
			http_request_print(&session->http_request);
			goto err;
		}

		assert(bytes_received > 0);

#ifdef LOG_HTTP_PARSER
		debuglog("http_parser_execute(%p, %p, %p, %zu\n)", parser, settings,
		         &session->request.buffer[session->request.length], bytes_received);
#endif
		size_t bytes_parsed = http_parser_execute(parser, settings,
		                                          (const char *)&request->buffer[request_length],
		                                          (size_t)bytes_received);

		if (bytes_parsed != (size_t)bytes_received) {
			debuglog("Error: %s, Description: %s\n",
			         http_errno_name((enum http_errno)session->http_parser.http_errno),
			         http_errno_description((enum http_errno)session->http_parser.http_errno));
			debuglog("Length Parsed %zu, Length Read %zu\n", bytes_parsed, (size_t)bytes_received);
			debuglog("Error parsing socket %d\n", session->socket);
			goto err;
		}

		request->length += bytes_parsed;
	}

#ifdef LOG_HTTP_PARSER
	for (int i = 0; i < session->http_request.query_params_count; i++) {
		debuglog("Argument %d, Len: %d, %.*s\n", i, session->http_request.query_params[i].value_length,
		         session->http_request.query_params[i].value_length,
		         session->http_request.query_params[i].value);
	}
#endif

	rc = 0;
done:
	return rc;
err_eagain:
	rc = -3;
	goto done;
err_nobufs:
	rc = -2;
	goto done;
err:
	rc = -1;
	goto done;
}
