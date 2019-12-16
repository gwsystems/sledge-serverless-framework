#ifndef SFRT_HTTP_H
#define SFRT_HTTP_H

#include <http_parser.h>
#include <types.h>
#include <uv.h>

/* all in-memory ptrs.. don't mess around with that! */
struct http_header {
	char *key;
	char *val;
};

struct http_resp_header {
	char *hdr;
	int len;
};

struct http_request {
	struct http_header headers[HTTP_HEADERS_MAX];
	int nheaders;
	char *body;
	int bodylen;
	// additional for http-parser
	int last_was_value;
	int header_end;
	int message_begin, message_end;
};

struct http_response {
	struct http_resp_header headers[HTTP_HEADERS_MAX];
	int nheaders;
	char *body;
	int bodylen;
	char *status;
	int stlen;
	uv_buf_t bufs[HTTP_HEADERS_MAX * 2 + 3]; //max headers, one line for status code, remaining for body!
};

int http_request_body_get(char **body);
int http_request_parse(void);

int http_response_header_set(char *h, int len);
int http_response_body_set(char *body, int len);
int http_response_status_set(char *status, int len);
int http_response_uv(void);

void http_init(void);

#endif /* SFRT_HTTP_H */
