#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "http.h"

/* all in-memory ptrs.. don't mess around with that! */
struct http_header {
	char *key;
	int   key_length;
	char *value;
	int   value_length;
};

struct http_request {
	struct http_header headers[HTTP_MAX_HEADER_COUNT];
	int                header_count;
	char *             body;
	int                body_length;
	int                body_read_length; /* How far we've read */

	/* additional members for http-parser */
	bool last_was_value; /* http-parser flag used to help the http-parser callbacks differentiate between header
	                       fields and values to know when to allocate a new header */
	bool header_end;     /* boolean flag set when header processing is complete */
	bool message_begin;  /* boolean flag set when body processing begins */
	bool message_end;    /* boolean flag set when body processing is complete */
};

static inline void
http_request_print(struct http_request *self)
{
	printf("Header Count %d\n", self->header_count);
	printf("Header Content:\n");
	for (int i = 0; i < self->header_count; i++) {
		for (int j = 0; j < self->headers[i].key_length; j++) { putchar(self->headers[i].key[j]); }
		putchar(':');
		for (int j = 0; j < self->headers[i].value_length; j++) { putchar(self->headers[i].value[j]); }
		putchar('\n');
	}
	printf("Body Length %d\n", self->body_length);
	printf("Body Read Length %d\n", self->body_read_length);
}

/***************************************************
 * General HTTP Request Functions                  *
 **************************************************/
int http_request_get_body(struct http_request *http_request, char **body);
