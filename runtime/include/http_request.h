#pragma once

#include <stdbool.h>

#include "http.h"

/* all in-memory ptrs.. don't mess around with that! */
struct http_header {
	char *key;
	int   key_length;
	char *value;
	int   value_length;
};

struct http_query_param {
	char value[HTTP_MAX_QUERY_PARAM_LENGTH];
	int  value_length;
};

struct http_request {
	char                    full_url[HTTP_MAX_FULL_URL_LENGTH];
	struct http_header      headers[HTTP_MAX_HEADER_COUNT];
	int                     header_count;
	struct http_query_param query_params[HTTP_MAX_QUERY_PARAM_COUNT];
	int                     query_params_count;
	int                     header_length;
	char                   *body;
	int                     body_length;
	int                     body_length_read; /* Amount read into buffer from socket */

	/* additional members for http-parser */
	int  length_parsed;  /* Amount parsed */
	bool last_was_value; /* http-parser flag used to help the http-parser callbacks differentiate between header
	                       fields and values to know when to allocate a new header */
	bool header_end;     /* boolean flag set when header processing is complete */
	bool message_begin;  /* boolean flag set when body processing begins */
	bool message_end;    /* boolean flag set when body processing is complete */

	/* Runtime state used by WASI */
	int cursor; /* Sandbox cursor (offset from body pointer) */
};

void http_request_print(struct http_request *http_request);
