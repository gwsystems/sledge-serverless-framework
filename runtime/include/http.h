#ifndef SFRT_HTTP_H
#define SFRT_HTTP_H

#include <http_parser.h>
#include <types.h>
#include <uv.h>
#include <sys/uio.h>

/* all in-memory ptrs.. don't mess around with that! */
struct http_header {
	char *key;
	char *val;
};

struct http_response_header {
	char *header;
	int   length;
};

struct http_request {
	struct http_header headers[HTTP_HEADERS_MAX];
	int                header_count;
	char *             body;
	int                body_length;
	// TODO: What does bodyrlen mean? Does this suggest that I've misunderstood what bodylen was?
	int bodyrlen;
	// additional for http-parser
	int last_was_value;
	int header_end;
	int message_begin, message_end;
};

struct http_response {
	struct http_response_header headers[HTTP_HEADERS_MAX];
	int                         header_count;
	char *                      body;
	int                         body_length;
	char *                      status;
	int                         status_length;
#ifdef USE_HTTP_UVIO
	uv_buf_t bufs[HTTP_HEADERS_MAX * 2 + 3]; // max headers, one line for status code, remaining for body!
#else
	struct iovec bufs[HTTP_HEADERS_MAX * 2 + 3];
#endif
};

#endif /* SFRT_HTTP_H */
