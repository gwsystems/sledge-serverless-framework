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
	struct auto_buf         request_buffer;
	struct auto_buf         response_header;
	size_t                  response_header_written;
	struct auto_buf         response_body;
	size_t                  response_body_written;
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

	int rc = auto_buf_init(&session->request_buffer);
	if (rc < 0) return -1;

	/* Defer initializing response_body until we've matched a route */
	auto_buf_init(&session->response_header);

	session->state = HTTP_SESSION_INITIALIZED;

	return 0;
}

static inline int
http_session_init_response_body(struct http_session *session)
{
	assert(session != NULL);
	assert(session->response_body.data == NULL);
	assert(session->response_body.size == 0);
	assert(session->response_body_written == 0);

	int rc = auto_buf_init(&session->response_body);
	if (rc < 0) {
		auto_buf_deinit(&session->request_buffer);
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

	auto_buf_deinit(&session->request_buffer);
	auto_buf_deinit(&session->response_header);
	auto_buf_deinit(&session->response_body);
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

	int rc = fputs(http_header_build(status_code), session->response_header.handle);
	assert(rc != EOF);

	if (status_code == 200) {
		/* Make sure the response_body is flushed */
		int rc = auto_buf_flush(&session->response_body);
		if (unlikely(rc != 0)) { panic("response_body auto_buf failed to flush: %s\n", strerror(errno)); };

		/* Technically fprintf can truncate, but I assume this won't happen with a memstream */
		rc = fprintf(session->response_header.handle, HTTP_RESPONSE_CONTENT_TYPE,
		             session->route->response_content_type);
		assert(rc > 0);
		rc = fprintf(session->response_header.handle, HTTP_RESPONSE_CONTENT_LENGTH,
		             session->response_body.size);
		assert(rc > 0);
	}

	rc = fputs(HTTP_RESPONSE_TERMINATOR, session->response_header.handle);
	assert(rc != EOF);

	rc = auto_buf_flush(&session->response_header);
	if (unlikely(rc != 0)) { panic("response_header auto_buf failed to flush: %s\n", strerror(errno)); };

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

	while (session->response_header.size > session->response_header_written) {
		ssize_t sent =
		  tcp_session_send(session->socket,
		                   (const char *)&session->response_header.data[session->response_header_written],
		                   session->response_header.size - session->response_header_written, on_eagain,
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

	while (session->response_body_written < session->response_body.size) {
		ssize_t sent =
		  tcp_session_send(session->socket,
		                   (const char *)&session->response_body.data[session->response_body_written],
		                   session->response_body.size - session->response_body_written, on_eagain, session);
		if (sent < 0) {
			return (int)sent;
		} else {
			session->response_body_written += (size_t)sent;
		}
	}

	session->state = HTTP_SESSION_SENT_RESPONSE_BODY;
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
	                      (const char *)&session->request_buffer.data[session->http_request.length_parsed],
	                      (size_t)session->request_buffer.size - session->http_request.length_parsed);

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
 * @return 0 if message parsing complete, -1 on error, -EAGAIN if would block
 */
static inline int
http_session_receive_request(struct http_session *session, void_star_cb on_eagain)
{
	assert(session != NULL);
	assert(session->request_buffer.handle != NULL);
	assert(session->state == HTTP_SESSION_INITIALIZED || session->state == HTTP_SESSION_RECEIVE_REQUEST_BLOCKED);

	session->state = HTTP_SESSION_RECEIVING_REQUEST;

	struct http_request *http_request = &session->http_request;
	int                  rc           = 0;
	char                 temp[BUFSIZ];

	while (!http_request->message_end) {
		ssize_t bytes_received = tcp_session_recv(session->socket, temp, BUFSIZ, on_eagain, session);
		if (unlikely(bytes_received == -EAGAIN))
			goto err_eagain;
		else if (unlikely(bytes_received < 0))
			goto err;
		/* If we received an EOF before we were able to parse a complete HTTP message, request is malformed */
		else if (unlikely(bytes_received == 0 && !http_request->message_end))
			goto err;

		assert(bytes_received > 0);

		const char   *old_buffer    = session->request_buffer.data;
		const ssize_t header_length = session->request_buffer.size - http_request->body_length_read;
		assert(!http_request->header_end || header_length > 0);

		/* Write temp buffer to memstream */
		fwrite(temp, 1, bytes_received, session->request_buffer.handle);

		/* fflush memstream managed buffer */
		fflush(session->request_buffer.handle);

		/* Update parser structure if buffer moved */
		if (old_buffer != session->request_buffer.data) {
			http_request->body = header_length ? session->request_buffer.data + header_length : NULL;
		}

		if (http_session_parse(session, bytes_received) == -1) goto err;
	}

	assert(http_request->message_end == true);
	session->state = HTTP_SESSION_RECEIVED_REQUEST;

	http_session_log_query_params(session);

	rc = 0;
done:
	return rc;
err_eagain:
	rc = -EAGAIN;
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
	assert(session->response_body.handle != NULL);
	assert(source);

	return fwrite(source, 1, n, session->response_body.handle);
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
