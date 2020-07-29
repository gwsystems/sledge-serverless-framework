#pragma once

#include <http_parser.h>
#include <sys/uio.h>
/* Conditionally load libuv */
#ifdef USE_HTTP_UVIO
#include <uv.h>
#endif

#include "http.h"

#define HTTP_RESPONSE_200_OK                    "HTTP/1.1 200 OK\r\n"
#define HTTP_RESPONSE_CONTENT_LENGTH            "Content-Length: "
#define HTTP_RESPONSE_CONTENT_LENGTH_TERMINATOR "\r\n\r\n" /* content body follows this */
#define HTTP_RESPONSE_CONTENT_TYPE              "Content-Type: "
#define HTTP_RESPONSE_CONTENT_TYPE_PLAIN        "text/plain"
#define HTTP_RESPONSE_CONTENT_TYPE_TERMINATOR   " \r\n"

struct http_response_header {
	char *header;
	int   length;
};

struct http_response {
	struct http_response_header headers[HTTP_MAX_HEADER_COUNT];
	int                         header_count;
	char *                      body;
	int                         body_length;
	char *                      status;
	int                         status_length;
#ifdef USE_HTTP_UVIO
	uv_buf_t bufs[HTTP_MAX_HEADER_COUNT * 2 + 3]; /* max headers, one line for status code, remaining for body! */
#else
	struct iovec bufs[HTTP_MAX_HEADER_COUNT * 2 + 3];
#endif
};

/***************************************************
 * General HTTP Response Functions                 *
 **************************************************/
int http_response_encode_as_vector(struct http_response *http_response);
int http_response_set_body(struct http_response *http_response, char *body, int length);
int http_response_set_header(struct http_response *http_response, char *h, int length);
int http_response_set_status(struct http_response *http_response, char *status, int length);
