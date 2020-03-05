#include <http.h>
#include <sandbox.h>
#include <uv.h>
#include <http_api.h>

http_parser_settings settings;

/**
 * Sets the HTTP Request's message_begin and last_was_value flags to true
 * @param parser
 **/
static inline int
http_on_msg_begin(http_parser *parser)
{
	struct sandbox *     sandbox = parser->data;
	struct http_request *http_request = &sandbox->http_request;

	http_request->message_begin  = 1;
	http_request->last_was_value = 1; // should always start with a header..
	return 0;
}

/**
 * Sets the HTTP Request's message_end flag to true
 * @param parser
 **/
static inline int
http_on_msg_end(http_parser *parser)
{
	struct sandbox *     sandbox = parser->data;
	struct http_request *http_request = &sandbox->http_request;

	http_request->message_end = 1;
	return 0;
}

/**
 * Sets the HTTP Request's header_end flag to true
 * @param parser
 **/
static inline int
http_on_header_end(http_parser *parser)
{
	struct sandbox *     sandbox = parser->data;
	struct http_request *http_request = &sandbox->http_request;

	http_request->header_end = 1;
	return 0;
}

/**
 * ???
 * @param parser
 * @param at
 * @param length
 * @returns 0
 **/
static inline int
http_on_url(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *     sandbox = parser->data;

	assert(strncmp(sandbox->module->name, (at + 1), length - 1) == 0);
	return 0;
}

/**
 * ???
 * @param parser
 * @param at
 * @param length
 * @returns 0
 **/
static inline int
http_on_header_field(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *     sandbox = parser->data;
	struct http_request *http_request = &sandbox->http_request;

	if (http_request->last_was_value) http_request->nheaders++;
	assert(http_request->nheaders <= HTTP_HEADERS_MAX);
	assert(length < HTTP_HEADER_MAXSZ);

	http_request->last_was_value               = 0;
	http_request->headers[http_request->nheaders - 1].key = (char *)at; // it is from the sandbox's req_resp_data, should persist.

	return 0;
}

/**
 * ???
 * @param parser
 * @param at
 * @param length
 * @returns 0
 **/
static inline int
http_on_header_value(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *     sandbox = parser->data;
	struct http_request *http_request = &sandbox->http_request;

	http_request->last_was_value = 1;
	assert(http_request->nheaders <= HTTP_HEADERS_MAX);
	assert(length < HTTP_HEADERVAL_MAXSZ);

	http_request->headers[http_request->nheaders - 1].val = (char *)at; // it is from the sandbox's req_resp_data, should persist.

	return 0;
}

/**
 * ???
 * @param parser
 * @param at
 * @param length
 * @returns 0
 **/
static inline int
http_on_body(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *     sandbox = parser->data;
	struct http_request *http_request = &sandbox->http_request;

	assert(http_request->bodylen + length <= sandbox->module->max_request_size);
	if (!http_request->body)
		http_request->body = (char *)at;
	else
		assert(http_request->body + http_request->bodylen == at);

	http_request->bodylen += length;

	return 0;
}

/**
 * Gets the HTTP Request body from a Sandbox
 * @param sandbox the sandbox we want the body from
 * @param b pointer that we'll assign to the http_request body
 * @returns the length of the http_request's body
 **/
int
http_request_body_get_sb(struct sandbox *sandbox, char **b)
{
	struct http_request *http_request = &sandbox->http_request;

	*b = http_request->body;
	return http_request->bodylen;
}

int
http_response_header_set_sb(struct sandbox *sandbox, char *key, int len)
{
	// by now, req_resp_data should only be containing response!
	struct http_response *http_response = &sandbox->http_response;

	assert(http_response->nheaders < HTTP_HEADERS_MAX);
	http_response->nheaders++;
	http_response->headers[http_response->nheaders - 1].hdr = key;
	http_response->headers[http_response->nheaders - 1].len = len;

	return 0;
}

int
http_response_body_set_sb(struct sandbox *sandbox, char *body, int len)
{
	struct http_response *http_response = &sandbox->http_response;

	assert(len <= sandbox->module->max_response_size);
	http_response->body    = body;
	http_response->bodylen = len;

	return 0;
}

int
http_response_status_set_sb(struct sandbox *sandbox, char *status, int len)
{
	struct http_response *http_response = &sandbox->http_response;

	http_response->status = status;
	http_response->stlen  = len;

	return 0;
}

int
http_response_vector_sb(struct sandbox *sandbox)
{
	int nb = 0;
	struct http_response *http_response = &sandbox->http_response;

#ifdef USE_HTTP_UVIO

	http_response->bufs[nb] = uv_buf_init(http_response->status, http_response->stlen);
	nb++;
	for (int i = 0; i < http_response->nheaders; i++) {
		http_response->bufs[nb] = uv_buf_init(http_response->headers[i].hdr, http_response->headers[i].len);
		nb++;
	}

	if (http_response->body) {
		http_response->bufs[nb] = uv_buf_init(http_response->body, http_response->bodylen);
		nb++;
		http_response->bufs[nb] = uv_buf_init(http_response->status + http_response->stlen - 2, 2); // for crlf
		nb++;
	}
#else
	http_response->bufs[nb].iov_base = http_response->status;
	http_response->bufs[nb].iov_len  = http_response->stlen;
	nb++;

	for (int i = 0; i < http_response->nheaders; i++) {
		http_response->bufs[nb].iov_base = http_response->headers[i].hdr;
		http_response->bufs[nb].iov_len  = http_response->headers[i].len;
		nb++;
	}

	if (http_response->body) {
		http_response->bufs[nb].iov_base = http_response->body;
		http_response->bufs[nb].iov_len  = http_response->bodylen;
		nb++;

		http_response->bufs[nb].iov_base = http_response->status + http_response->stlen - 2;
		http_response->bufs[nb].iov_len  = 2;
		nb++;
	}
#endif

	return nb;
}

int
http_request_parse_sb(struct sandbox *sandbox, size_t l)
{
	http_parser_execute(&sandbox->http_parser, &settings, sandbox->req_resp_data + sandbox->rr_data_len, l);
	return 0;
}

/**
 * Configure Callback Functions for HTTP Events
 */
void
http_init(void)
{
	http_parser_settings_init(&settings);
	settings.on_url              = http_on_url;
	settings.on_header_field     = http_on_header_field;
	settings.on_header_value     = http_on_header_value;
	settings.on_body             = http_on_body;
	settings.on_headers_complete = http_on_header_end;
	settings.on_message_begin    = http_on_msg_begin;
	settings.on_message_complete = http_on_msg_end;
}
