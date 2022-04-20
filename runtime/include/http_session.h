#pragma once

#include <sys/socket.h>
#include <stdint.h>

#include "http_request.h"
#include "http_parser.h"
#include "vec.h"

#define u8 uint8_t
VEC(u8)

struct http_session {
	/* HTTP State */
	struct sockaddr     client_address; /* client requesting connection! */
	int                 client_socket_descriptor;
	http_parser         http_parser;
	struct http_request http_request;
	struct vec_u8       request;
	struct vec_u8       response;
};

static inline void
http_session_init_parser(struct http_session *session)
{
	assert(session != NULL);

	http_parser_init(&session->http_parser, HTTP_REQUEST);

	/* Set the session as the data the http-parser has access to */
	session->http_parser.data = &session->http_request;
}
