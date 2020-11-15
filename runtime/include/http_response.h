#pragma once

#include <http_parser.h>
#include <sys/uio.h>

#include "http.h"

#define HTTP_RESPONSE_200_OK                    "HTTP/1.1 200 OK\r\n"
#define HTTP_RESPONSE_503_SERVICE_UNAVAILABLE   "HTTP/1.1 503 Service Unavailable\r\n\r\n"
#define HTTP_RESPONSE_400_BAD_REQUEST           "HTTP/1.1 400 Bad Request\r\n\r\n"
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
	struct iovec                bufs[HTTP_MAX_HEADER_COUNT * 2 + 3];
};

/***************************************************
 * General HTTP Response Functions                 *
 **************************************************/
int http_response_encode_as_vector(struct http_response *http_response);
int http_response_set_body(struct http_response *http_response, char *body, int length);
int http_response_set_header(struct http_response *http_response, char *h, int length);
int http_response_set_status(struct http_response *http_response, char *status, int length);
