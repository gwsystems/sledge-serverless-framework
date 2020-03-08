#include <http.h>
#include <sandbox.h>
#include <uv.h>
#include <http_api.h>

http_parser_settings settings;

/***************************************************
 * General HTTP Request Functions                  *
 **************************************************/

/**
 * Gets the HTTP Request body from a Sandbox
 * @param sandbox the sandbox we want the body from
 * @param body pointer that we'll assign to the http_request body
 * @returns the length of the http_request's body
 **/
int
http_request_body_get_sb(struct sandbox *sandbox, char **body)
{
	struct http_request *http_request = &sandbox->http_request;

	*body = http_request->body;
	return http_request->body_length;
}

/***************************************************
 * General HTTP Response Functions                 *
 **************************************************/

/**
 * Append a header to the HTTP Response of a Sandbox
 * @param sandbox the sandbox we want to set the request header on
 * @param header string containing the header that we want to append
 * @param length the length of the header string
 * @returns 0 (abends program in case of error)
 **/
int
http_response_header_set_sb(struct sandbox *sandbox, char *header, int length)
{
	// by now, req_resp_data should only be containing response!
	struct http_response *http_response = &sandbox->http_response;

	assert(http_response->header_count < HTTP_HEADERS_MAX);
	http_response->header_count++;
	http_response->headers[http_response->header_count - 1].header = header;
	http_response->headers[http_response->header_count - 1].length = length;

	return 0;
}

/**
 * Set an HTTP Response Body on a Sandbox
 * @param sandbox the sandbox we want to set the request header on
 * @param body string of the body that we want to set
 * @param length the length of the header string
 * @returns 0 (abends program in case of error)
 **/
int
http_response_body_set_sb(struct sandbox *sandbox, char *body, int length)
{
	struct http_response *http_response = &sandbox->http_response;

	assert(length <= sandbox->module->max_response_size);
	http_response->body        = body;
	http_response->body_length = length;

	return 0;
}

/**
 * Set an HTTP Response Status on a Sandbox
 * @param sandbox the sandbox we want to set the request status on
 * @param status string of the status we want to set
 * @param length the length of the status
 * @returns 0 (abends program in case of error)
 **/
int
http_response_status_set_sb(struct sandbox *sandbox, char *status, int length)
{
	struct http_response *http_response = &sandbox->http_response;

	http_response->status        = status;
	http_response->status_length = length;

	return 0;
}

/**
 * Encodes a sandbox's HTTP Response as an array of buffers
 * @param sandbox the sandbox containing the HTTP response we want to encode as buffers
 * @returns the number of buffers used to store the HTTP Response
 **/
int
http_response_vector_sb(struct sandbox *sandbox)
{
	int buffer_count = 0;
	struct http_response *http_response = &sandbox->http_response;

#ifdef USE_HTTP_UVIO

	http_response->bufs[buffer_count] = uv_buf_init(http_response->status, http_response->status_length);
	buffer_count++;
	for (int i = 0; i < http_response->header_count; i++) {
		http_response->bufs[buffer_count] = uv_buf_init(http_response->headers[i].header, http_response->headers[i].length);
		buffer_count++;
	}

	if (http_response->body) {
		http_response->bufs[buffer_count] = uv_buf_init(http_response->body, http_response->body_length);
		buffer_count++;
		http_response->bufs[buffer_count] = uv_buf_init(http_response->status + http_response->status_length - 2, 2); // for crlf
		buffer_count++;
	}
#else
	http_response->bufs[buffer_count].iov_base = http_response->status;
	http_response->bufs[buffer_count].iov_len  = http_response->status_length;
	buffer_count++;

	for (int i = 0; i < http_response->header_count; i++) {
		http_response->bufs[buffer_count].iov_base = http_response->headers[i].hdr;
		http_response->bufs[buffer_count].iov_len  = http_response->headers[i].len;
		buffer_count++;
	}

	if (http_response->body) {
		http_response->bufs[buffer_count].iov_base = http_response->body;
		http_response->bufs[buffer_count].iov_len  = http_response->body_length;
		buffer_count++;

		http_response->bufs[buffer_count].iov_base = http_response->status + http_response->status_length - 2;
		http_response->bufs[buffer_count].iov_len  = 2;
		buffer_count++;
	}
#endif

	return buffer_count;
}

/***********************************************************************
 * http-parser Callbacks in lifecycle order                            *
 **********************************************************************/

/**
 * http-parser data callback called when a URL is called
 * Sanity check to make sure that the path matches the name of the module
 * TODO: Why does this not fail this assertion? To execute fibonacci, I just request localhost:10000, not localhost:10000/fibonacci
 * @param parser
 * @param at the start of the URL
 * @param length the length of the URL
 * @returns 0
 **/
static inline int
http_on_url(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *sandbox = parser->data;

	assert(strncmp(sandbox->module->name, (at + 1), length - 1) == 0);
	return 0;
}

/**
 * http-parser callback called when parsing of a new message begins
 * Sets the HTTP Request's message_begin and last_was_value flags to true
 * @param parser
 **/
static inline int
http_on_msg_begin(http_parser *parser)
{
	struct sandbox *     sandbox      = parser->data;
	struct http_request *http_request = &sandbox->http_request;

	http_request->message_begin  = 1;
	http_request->last_was_value = 1; // should always start with a header..
	return 0;
}

/**
 * http-parser callback called when a header field is parsed
 * Sets the key value of the latest header
 * on a new header if last_was_value is true
 * updating an existing header if last_was_value is false
 * TODO: Is this logic correct? What is the relationship between fields and values? Is overwrite the correct logic if on_header executes twice in a row?
 * @param parser
 * @param at start address of the header field
 * @param length length of the header field
 * @returns 0
 **/
static inline int
http_on_header_field(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *     sandbox      = parser->data;
	struct http_request *http_request = &sandbox->http_request;

	if (http_request->last_was_value) http_request->header_count++;
	assert(http_request->header_count <= HTTP_HEADERS_MAX);
	assert(length < HTTP_HEADER_MAXSZ);

	http_request->last_was_value               = 0;
	http_request->headers[http_request->header_count - 1].key = (char *)at; // it is from the sandbox's req_resp_data, should persist.

	return 0;
}

/**
 * http-parser callback called when a header value is parsed
 * Writes the value to the latest header and sets last_was_value to true
 * @param parser
 * @param at start address of the header value
 * @param length length of the header value
 * @returns 0
 **/
static inline int
http_on_header_value(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *     sandbox      = parser->data;
	struct http_request *http_request = &sandbox->http_request;

	http_request->last_was_value = 1;
	assert(http_request->header_count <= HTTP_HEADERS_MAX);
	assert(length < HTTP_HEADERVAL_MAXSZ);

	http_request->headers[http_request->header_count - 1].value = (char *)at; // it is from the sandbox's req_resp_data, should persist.

	return 0;
}

/**
 * http-parser callback called when header parsing is complete
 * Just sets the HTTP Request's header_end flag to true
 * @param parser
 **/
static inline int
http_on_header_end(http_parser *parser)
{
	struct sandbox *     sandbox      = parser->data;
	struct http_request *http_request = &sandbox->http_request;

	http_request->header_end = 1;
	return 0;
}

/**
 * http-parser callback called for HTTP Bodies
 * Assigns the parsed data to the http_request body of the sandbox struct
 * @param parser
 * @param at
 * @param length
 * @returns 0
 **/
static inline int
http_on_body(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *     sandbox      = parser->data;
	struct http_request *http_request = &sandbox->http_request;

	assert(http_request->body_length + length <= sandbox->module->max_request_size);
	if (!http_request->body)
		http_request->body = (char *)at;
	else
		assert(http_request->body + http_request->body_length == at);

	http_request->body_length += length;

	return 0;
}

/**
 * Sets the HTTP Request's message_end flag to true
 * @param parser
 **/
static inline int
http_on_msg_end(http_parser *parser)
{
	struct sandbox *     sandbox      = parser->data;
	struct http_request *http_request = &sandbox->http_request;

	http_request->message_end = 1;
	return 0;
}

/***********************************************************************
 * http-parser Setup and Excute                                        *
 **********************************************************************/

/**
 * The settings global with the Callback Functions for HTTP Events
 */
void
http_init(void)
{
	http_parser_settings_init(&settings);
	settings.on_url              = http_on_url;
	settings.on_message_begin    = http_on_msg_begin;
	settings.on_header_field     = http_on_header_field;
	settings.on_header_value     = http_on_header_value;
	settings.on_headers_complete = http_on_header_end;
	settings.on_body             = http_on_body;
	settings.on_message_complete = http_on_msg_end;
}

/**
 * Run the http-parser on the sandbox's req_resp_data using the configured settings global
 * @param sandbox the sandbox containing the req_resp data that we want to parse
 * @param length The size of the req_resp_data that we want to parse
 * @returns 0
 * 
 * Global State: settings
 **/
int
http_request_parse_sb(struct sandbox *sandbox, size_t length)
{
	// TODO: Why is our start address sandbox->req_resp_data + sandbox->rr_data_len?
	http_parser_execute(&sandbox->http_parser, &settings, sandbox->req_resp_data + sandbox->rr_data_len, length);
	return 0;
}
