#ifndef SFRT_HTTP_H
#define SFRT_HTTP_H

#include <http_parser.h>
#include <types.h>
#include <sys/uio.h>

// Conditionally load libuv
#ifdef USE_HTTP_UVIO
#include <uv.h>
#endif

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
	int 			   body_read_length; // How far we've read
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

/***************************************************
 * General HTTP Request Functions                  *
 **************************************************/
int http_request__get_body(struct http_request *http_request, char **body);


/***************************************************
 * General HTTP Response Functions                 *
 **************************************************/
int http_response__encode_as_vector(struct http_response *http_response);
int http_response__set_body(struct http_response *http_response, char *body, int length);
int http_response__set_header(struct http_response *http_response, char *h, int length);
int http_response__set_status(struct http_response *http_response, char *status, int length);

#endif /* SFRT_HTTP_H */
