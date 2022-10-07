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

static inline void http_header_copy(struct http_header *dest, struct http_header *source) {
	if (source == NULL || dest == NULL) return;

	dest->key = (char*) malloc (source->key_length);	
	memcpy(dest->key, source->key, source->key_length);
	dest->key_length = source->key_length;

	dest->value = (char*) malloc (source->value_length);
	memcpy(dest->value, source->value, source->value_length);
	dest->value_length = source->value_length;
}

struct http_query_param {
	char value[HTTP_MAX_QUERY_PARAM_LENGTH];
	int  value_length;
};

static inline void http_query_param_copy(struct http_query_param *dest, struct http_query_param *source) {
	if (source == NULL || dest == NULL) return;
	memcpy(dest->value, source->value, HTTP_MAX_QUERY_PARAM_LENGTH);
	dest->value_length = source->value_length;
}
struct http_request {
	char                    full_url[HTTP_MAX_FULL_URL_LENGTH];
	struct http_header      headers[HTTP_MAX_HEADER_COUNT];
	int                     header_count;
	uint32_t                method;
	struct http_query_param query_params[HTTP_MAX_QUERY_PARAM_COUNT];
	int                     query_params_count;
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

static inline void http_request_copy(struct http_request *dest, struct http_request *source) {

	if (dest == NULL || source == NULL) return;
	memcpy(dest->full_url, source->full_url, HTTP_MAX_FULL_URL_LENGTH);
	for (int i = 0; i < HTTP_MAX_HEADER_COUNT; i++) {
		http_header_copy(&(dest->headers[i]), &(source->headers[i]));
	}
	dest->header_count = source->header_count;
	dest->method = source->method;

	for (int i = 0; i < HTTP_MAX_QUERY_PARAM_COUNT; i++) {
		http_query_param_copy(&(dest->query_params[i]), &(source->query_params[i]));
	}
	dest->query_params_count = source->query_params_count;
	dest->body = (char*) malloc (source->body_length);
	memcpy(dest->body, source->body, source->body_length);
	dest->body_length = source->body_length;
	dest->body_length_read = source->body_length_read;
	dest->length_parsed = source->length_parsed;
	dest->last_was_value = source->last_was_value;
	dest->header_end = source->header_end;
	dest->message_begin = source->message_begin;
	dest->message_end = source->message_end;

}
void http_request_print(struct http_request *http_request);
