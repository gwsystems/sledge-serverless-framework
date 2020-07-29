#pragma once

#include "http.h"

/* all in-memory ptrs.. don't mess around with that! */
struct http_header {
	char *key;
	char *value;
};

struct http_request {
	struct http_header headers[HTTP_MAX_HEADER_COUNT];
	int                header_count;
	char *             body;
	int                body_length;
	int                body_read_length; /* How far we've read */

	/* additional members for http-parser */
	int last_was_value; /* http-parser flag used to help the http-parser callbacks differentiate between header
	                       fields and values to know when to allocate a new header */
	int header_end;     /* boolean flag set when header processing is complete */
	int message_begin;  /* boolean flag set when body processing begins */
	int message_end;    /* boolean flag set when body processing is complete */
};

/***************************************************
 * General HTTP Request Functions                  *
 **************************************************/
int http_request_get_body(struct http_request *http_request, char **body);
