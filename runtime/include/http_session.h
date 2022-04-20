#pragma once

#include <sys/socket.h>
#include <stdint.h>

#include "client_socket.h"
#include "http_request.h"
#include "http_parser.h"
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
