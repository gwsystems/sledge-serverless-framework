#ifndef SFRT_HTTP_H
#define SFRT_HTTP_H

#include <http_parser.h>
#include <types.h>
#include <uv.h>
#include <sys/uio.h>

/* all in-memory ptrs.. don't mess around with that! */
struct http_header {
	char *key;
	char *value;
};

struct http_request {
	struct http_header headers[HTTP_HEADERS_MAX];
	int                header_count;
	char *             body;
	int                body_length;
	// TODO: What does bodyrlen mean? Does this suggest that I've misunderstood what body_length is?
	int 			   bodyrlen;
	// additional for http-parser
	int last_was_value; // http-parser flag used to help the http-parser callbacks differentiate between header fields and values to know when to allocate a new header
	int header_end;     // boolean flag set when header processing is complete
	int message_begin;  // boolean flag set when body processing begins
	int message_end;    // boolean flag set when body processing is complete
};

struct http_response_header {
	char *header;
	int   length;
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
