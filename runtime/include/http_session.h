#pragma once

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "auto_buf.h"
#include "debuglog.h"
#include "epoll_tag.h"
#include "http_parser.h"
#include "http_parser_settings.h"
#include "http_request.h"
#include "http_route_total.h"
#include "http_session_perf_log.h"
#include "http_total.h"
#include "route.h"
#include "tcp_session.h"
#include "tenant.h"
#include "vec.h"

#define HTTP_SESSION_DEFAULT_REQUEST_SIZE     (PAGE_SIZE)
#define HTTP_SESSION_RESPONSE_HEADER_CAPACITY 256

#define u8 uint8_t
VEC(u8)

enum http_session_state
{
	HTTP_SESSION_UNINITIALIZED = 0,
	HTTP_SESSION_INITIALIZED,
	HTTP_SESSION_RECEIVING_REQUEST,
	HTTP_SESSION_RECEIVE_REQUEST_BLOCKED,
	HTTP_SESSION_RECEIVED_REQUEST,
	HTTP_SESSION_EXECUTING,
	HTTP_SESSION_EXECUTION_COMPLETE,
	HTTP_SESSION_SENDING_RESPONSE_HEADER,
	HTTP_SESSION_SEND_RESPONSE_HEADER_BLOCKED,
	HTTP_SESSION_SENT_RESPONSE_HEADER,
	HTTP_SESSION_SENDING_RESPONSE_BODY,
	HTTP_SESSION_SEND_RESPONSE_BODY_BLOCKED,
	HTTP_SESSION_SENT_RESPONSE_BODY
};

struct http_session {
	enum epoll_tag          tag;
	enum http_session_state state;
	struct sockaddr         client_address; /* client requesting connection! */
	int                     socket;
	struct http_parser      http_parser;
	struct http_request     http_request;
	struct vec_u8           request_buffer;
	char                    response_header[HTTP_SESSION_RESPONSE_HEADER_CAPACITY];
	size_t                  response_header_length;
	size_t                  response_header_written;
	struct auto_buf         response_buffer;
	size_t                  response_buffer_written;
	struct tenant          *tenant; /* Backlink required when read blocks on listener core */
	struct route           *route;  /* Backlink required to handle http metrics */
	uint64_t                request_arrival_timestamp;
	uint64_t                request_downloaded_timestamp;
	uint64_t                response_takeoff_timestamp;
	uint64_t                response_sent_timestamp;
};

extern void http_session_perf_log_print_entry(struct http_session *http_session);

/**
 * Initalize state associated with an http parser
 * Because the http_parser structure uses pointers to the request buffer, if realloc moves the request
 * buffer, this should be called to clear stale state to force parsing to restart
 */
static inline void
http_session_parser_init(struct http_session *session)
{
	memset(&session->http_request, 0, sizeof(struct http_request));
	http_parser_init(&session->http_parser, HTTP_REQUEST);
	/* Set the session as the data the http-parser has access to */
	session->http_parser.data = &session->http_request;
}

/**
 * @param session session that we want to init
 * @returns 0 on success, -1 on error
 */
static inline int
http_session_init(struct http_session *session, int socket_descriptor, const struct sockaddr *socket_address,
                  struct tenant *tenant, uint64_t request_arrival_timestamp)
{
	assert(session != NULL);
	assert(session->state == HTTP_SESSION_UNINITIALIZED);
	assert(socket_descriptor >= 0);
	assert(socket_address != NULL);

	session->tag                       = EPOLL_TAG_HTTP_SESSION_CLIENT_SOCKET;
	session->tenant                    = tenant;
	session->route                     = NULL;
	session->socket                    = socket_descriptor;
	session->request_arrival_timestamp = request_arrival_timestamp;
	memcpy(&session->client_address, socket_address, sizeof(struct sockaddr));

	http_session_parser_init(session);

	int rc = vec_u8_init(&session->request_buffer, HTTP_SESSION_DEFAULT_REQUEST_SIZE);
	if (rc < 0) return -1;

	/* Defer initializing response_buffer until we've matched a route */

	session->state = HTTP_SESSION_INITIALIZED;

	return 0;
}

static inline int
http_session_init_response_buffer(struct http_session *session)
{
	assert(session != NULL);
	assert(session->response_buffer.data == NULL);
	assert(session->response_buffer.size == 0);
	assert(session->response_buffer_written == 0);

	int rc = auto_buf_init(&session->response_buffer);
	if (rc < 0) {
		vec_u8_deinit(&session->request_buffer);
		return -1;
	}

	return 0;
}

static inline struct http_session *
http_session_alloc(int socket_descriptor, const struct sockaddr *socket_address, struct tenant *tenant,
                   uint64_t request_arrival_timestamp)
{
	assert(socket_descriptor >= 0);
	assert(socket_address != NULL);

	struct http_session *session = calloc(1, sizeof(struct http_session));
	if (session == NULL) return NULL;

	int rc = http_session_init(session, socket_descriptor, socket_address, tenant, request_arrival_timestamp);
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

	vec_u8_deinit(&session->request_buffer);
	auto_buf_deinit(&session->response_buffer);
}

static inline void
http_session_free(struct http_session *session)
{
	assert(session);

	http_session_deinit(session);
	free(session);
}

/**
 * Set Response Header
 * @param session - the HTTP session we want to set the response header of
 * @param status_code
 */
static inline void
http_session_set_response_header(struct http_session *session, int status_code)
{
	assert(session != NULL);
	assert(status_code >= 200 && status_code <= 599);
	http_total_increment_response(status_code);

	/* We might not have actually matched a route */
	if (likely(session->route != NULL)) { http_route_total_increment(&session->route->metrics, status_code); }

	if (status_code == 200) {
		int rc = auto_buf_flush(&session->response_buffer);
		if (unlikely(rc != 0)) { panic("auto_buf failed to flush: %s\n", strerror(errno)); };

		session->response_header_length = snprintf(session->response_header,
		                                           HTTP_SESSION_RESPONSE_HEADER_CAPACITY,
		                                           HTTP_RESPONSE_200_TEMPLATE,
		                                           session->route->response_content_type,
		                                           session->response_buffer.size);
	} else {
		size_t header_len = http_header_len(status_code);
		size_t to_copy    = HTTP_SESSION_RESPONSE_HEADER_CAPACITY < header_len
		                      ? HTTP_SESSION_RESPONSE_HEADER_CAPACITY
		                      : header_len;

		strncpy(session->response_header, http_header_build(status_code), to_copy);
		session->response_header_length = to_copy;
	}

	session->response_takeoff_timestamp = __getcycles();
}

static inline void
http_session_close(struct http_session *session)
{
	assert(session != NULL);

	return tcp_session_close(session->socket, &session->client_address);
}

/**
 * Writes an HTTP header to the client
 * @param client_socket - the client
 * @param on_eagain - cb to execute when client socket returns EAGAIN. If NULL, error out
 * @returns 0 on success, -errno on error
 */
static inline int
http_session_send_response_header(struct http_session *session, void_star_cb on_eagain)
{
	assert(session != NULL);
	assert(session->state == HTTP_SESSION_EXECUTION_COMPLETE
	       || session->state == HTTP_SESSION_SEND_RESPONSE_HEADER_BLOCKED);
	session->state = HTTP_SESSION_SENDING_RESPONSE_HEADER;

	while (session->response_header_length > session->response_header_written) {
		ssize_t sent =
		  tcp_session_send(session->socket,
		                   (const char *)&session->response_header[session->response_header_written],
		                   session->response_header_length - session->response_header_written, on_eagain,
		                   session);
		if (sent < 0) {
			return (int)sent;
		} else {
			session->response_header_written += (size_t)sent;
		}
	}

	session->state = HTTP_SESSION_SENT_RESPONSE_HEADER;

	return 0;
}

/**
 * Writes an HTTP body to the client
 * @param client_socket - the client
 * @param on_eagain - cb to execute when client socket returns EAGAIN. If NULL, error out
 * @returns 0 on success, -errno on error
 */
static inline int
http_session_send_response_body(struct http_session *session, void_star_cb on_eagain)
{
	assert(session != NULL);

	assert(session->state == HTTP_SESSION_SENT_RESPONSE_HEADER
	       || session->state == HTTP_SESSION_SEND_RESPONSE_BODY_BLOCKED);
	session->state = HTTP_SESSION_SENDING_RESPONSE_BODY;

	/* Assumption: Already flushed in order to write content-length to header */
	// TODO: Test if body is empty

	while (session->response_buffer_written < session->response_buffer.size) {
		ssize_t sent =
		  tcp_session_send(session->socket,
		                   (const char *)&session->response_buffer.data[session->response_buffer_written],
		                   session->response_buffer.size - session->response_buffer_written, on_eagain,
		                   session);
		if (sent < 0) {
			return (int)sent;
		} else {
			session->response_buffer_written += (size_t)sent;
		}
	}

	session->state = HTTP_SESSION_SENT_RESPONSE_BODY;
	return 0;
}

static inline bool
http_session_request_buffer_is_full(struct http_session *session)
{
	return session->request_buffer.length == session->request_buffer.capacity;
}

static inline int
http_session_request_buffer_grow(struct http_session *session)
{
	/* We have not yet fully parsed the header, so we don't know content-length, so just grow
	 * (double) the buffer */
	uint8_t *old_buffer = session->request_buffer.buffer;

	if (vec_u8_grow(&session->request_buffer) != 0) {
		debuglog("Failed to grow request buffer\n");
		return -1;
	}

	/* buffer moved, so invalidate to reparse */
	if (old_buffer != session->request_buffer.buffer) { http_session_parser_init(session); }

	return 0;
}

static inline int
http_session_request_buffer_resize(struct http_session *session, int required_size)
{
	uint8_t *old_buffer = session->request_buffer.buffer;
	if (vec_u8_resize(&session->request_buffer, required_size) != 0) {
		debuglog("Failed to resize request vector to %d bytes\n", required_size);
		return -1;
	}

	/* buffer moved, so invalidate to reparse */
	if (old_buffer != session->request_buffer.buffer) { http_session_parser_init(session); }

	return 0;
}

typedef void (*http_session_cb)(struct http_session *);

static inline ssize_t
http_session_parse(struct http_session *session, ssize_t bytes_received)
{
	assert(session != 0);
	assert(bytes_received > 0);

	const http_parser_settings *settings = http_parser_settings_get();

#ifdef LOG_HTTP_PARSER
	debuglog("http_parser_execute(%p, %p, %p, %zu\n)", &session->http_parser, settings,
	         &session->request_buffer.buffer[session->request_buffer.length], bytes_received);
#endif
	size_t bytes_parsed =
	  http_parser_execute(&session->http_parser, settings,
	                      (const char *)&session->request_buffer.buffer[session->http_request.length_parsed],
	                      (size_t)session->request_buffer.length - session->http_request.length_parsed);

	if (session->http_parser.http_errno != HPE_OK) {
		debuglog("Error: %s, Description: %s\n",
		         http_errno_name((enum http_errno)session->http_parser.http_errno),
		         http_errno_description((enum http_errno)session->http_parser.http_errno));
		debuglog("Length Parsed %zu, Length Read %zu\n", bytes_parsed, (size_t)bytes_received);
		debuglog("Error parsing socket %d\n", session->socket);
		return -1;
	}

	session->http_request.length_parsed += bytes_parsed;

	return (ssize_t)bytes_parsed;
}

static inline void
http_session_log_query_params(struct http_session *session)
{
#ifdef LOG_HTTP_PARSER
	for (int i = 0; i < session->http_request.query_params_count; i++) {
		debuglog("Argument %d, Len: %d, %.*s\n", i, session->http_request.query_params[i].value_length,
		         session->http_request.query_params[i].value_length,
		         session->http_request.query_params[i].value);
	}
#endif
}

static inline void
http_session_log_malformed_request(struct http_session *session)
{
#ifndef NDEBUG
	char client_address_text[INET6_ADDRSTRLEN] = {};
	if (unlikely(inet_ntop(AF_INET, &session->client_address, client_address_text, INET6_ADDRSTRLEN) == NULL)) {
		debuglog("Failed to log client_address: %s", strerror(errno));
	}

	debuglog("socket: %d. Address: %s\n", session->socket, client_address_text);
	http_request_print(&session->http_request);
#endif
}

/**
 * Receive and Parse the Request for the current sandbox
 * @return 0 if message parsing complete, -1 on error, -ENOMEM if buffers run out of space, -3 EAGAIN if would block
 */
static inline int
http_session_receive_request(struct http_session *session, void_star_cb on_eagain)
{
	assert(session != NULL);
	assert(session->request_buffer.capacity > 0);
	assert(session->request_buffer.length <= session->request_buffer.capacity);
	assert(session->state == HTTP_SESSION_INITIALIZED || session->state == HTTP_SESSION_RECEIVE_REQUEST_BLOCKED);

	session->state = HTTP_SESSION_RECEIVING_REQUEST;

	int rc = 0;

	while (!session->http_request.message_end) {
		/* If we know the header size and content-length, resize exactly. Otherwise double */
		if (session->http_request.header_end && session->http_request.body) {
			int header_size   = (uint8_t *)session->http_request.body - session->request_buffer.buffer;
			int required_size = header_size + session->http_request.body_length;

			if (required_size > session->request_buffer.capacity) {
				rc = http_session_request_buffer_resize(session, required_size);
				if (rc != 0) goto err_nobufs;
			}
		} else if (http_session_request_buffer_is_full(session)) {
			rc = http_session_request_buffer_grow(session);
			if (rc != 0) goto err_nobufs;
		}

		ssize_t bytes_received =
		  tcp_session_recv(session->socket,
		                   (char *)&session->request_buffer.buffer[session->request_buffer.length],
		                   session->request_buffer.capacity - session->request_buffer.length, on_eagain,
		                   session);
		if (unlikely(bytes_received == -EAGAIN))
			goto err_eagain;
		else if (unlikely(bytes_received < 0))
			goto err;
		/* If we received an EOF before we were able to parse a complete HTTP message, request is malformed */
		else if (unlikely(bytes_received == 0 && !session->http_request.message_end))
			goto err;

		assert(bytes_received > 0);
		assert(session->request_buffer.length < session->request_buffer.capacity);

		session->request_buffer.length += bytes_received;

		ssize_t bytes_parsed = http_session_parse(session, bytes_received);
		if (bytes_parsed == -1) goto err;
	}

	assert(session->http_request.message_end == true);
	session->state = HTTP_SESSION_RECEIVED_REQUEST;

	http_session_log_query_params(session);

	rc = 0;
done:
	return rc;
err_eagain:
	rc = -EAGAIN;
	goto done;
err_nobufs:
	http_session_log_malformed_request(session);
	rc = -ENOMEM;
	goto done;
err:
	http_session_log_malformed_request(session);
	rc = -1;
	goto done;
}

/**
 * Writes to the HTTP response buffer
 * On success, the number of bytes written is returned.  On error, -1 is returned,
 */
static inline int
http_session_write_response(struct http_session *session, const uint8_t *source, size_t n)
{
	assert(session);
	assert(session->response_buffer.handle != NULL);
	assert(source);

	return fwrite(source, 1, n, session->response_buffer.handle);
}

static inline void
http_session_send_response(struct http_session *session, void_star_cb on_eagain)
{
	assert(session->state == HTTP_SESSION_EXECUTION_COMPLETE);

	int rc = http_session_send_response_header(session, on_eagain);
	/* session blocked and registered to epoll so continue to next handle */
	if (unlikely(rc == -EAGAIN)) {
		goto DONE;
	} else if (unlikely(rc < 0)) {
		goto CLOSE;
	}

	assert(session->state == HTTP_SESSION_SENT_RESPONSE_HEADER);

	rc = http_session_send_response_body(session, on_eagain);
	/* session blocked and registered to epoll so continue to next handle */
	if (unlikely(rc == -EAGAIN)) {
		goto DONE;
	} else if (unlikely(rc < 0)) {
		goto CLOSE;
	}

	assert(session->state == HTTP_SESSION_SENT_RESPONSE_BODY);

	/* Terminal State Logging for Http Session */
	session->response_sent_timestamp = __getcycles();
	http_session_perf_log_print_entry(session);

CLOSE:
	http_session_close(session);
	http_session_free(session);
DONE:
	return;
}
