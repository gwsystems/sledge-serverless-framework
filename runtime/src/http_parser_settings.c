#include <uv.h>

#include "http.h"
#include "http_request.h"
#include "http_response.h"
#include "http_parser_settings.h"
#include "sandbox.h"

static http_parser_settings runtime_http_parser_settings;

/***********************************************************************
 * http-parser Callbacks in lifecycle order                            *
 **********************************************************************/

/**
 * http-parser data callback called when a URL is called
 * Sanity check to make sure that the path matches the name of the module
 * @param parser
 * @param at the start of the URL
 * @param length the length of the URL
 * @returns 0
 */
int
http_parser_settings_on_url(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *sandbox = (struct sandbox *)parser->data;

	assert(strncmp(sandbox->module->name, (at + 1), length - 1) == 0);
	return 0;
}

/**
 * http-parser callback called when parsing of a new message begins
 * Sets the HTTP Request's message_begin and last_was_value flags to true
 * @param parser
 */
int
http_parser_settings_on_message_begin(http_parser *parser)
{
	struct sandbox *     sandbox      = (struct sandbox *)parser->data;
	struct http_request *http_request = &sandbox->http_request;

	http_request->message_begin  = 1;
	http_request->last_was_value = 1; /* should always start with a header */
	return 0;
}

/**
 * http-parser callback called when a header field is parsed
 * Sets the key value of the latest header on a new header if last_was_value is true
 * updating an existing header if last_was_value is false
 * @param parser
 * @param at start address of the header field
 * @param length length of the header field
 * @returns 0
 */
int
http_parser_settings_on_header_field(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *     sandbox      = (struct sandbox *)parser->data;
	struct http_request *http_request = &sandbox->http_request;

	if (http_request->last_was_value) http_request->header_count++;
	assert(http_request->header_count <= HTTP_MAX_HEADER_COUNT);
	assert(length < HTTP_MAX_HEADER_LENGTH);

	http_request->last_was_value = 0;

	/* it is from the sandbox's request_response_data, should persist. */
	http_request->headers[http_request->header_count - 1].key = (char *)at;

	return 0;
}

/**
 * http-parser callback called when a header value is parsed
 * Writes the value to the latest header and sets last_was_value to true
 * @param parser
 * @param at start address of the header value
 * @param length length of the header value
 * @returns 0
 */
int
http_parser_settings_on_header_value(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *     sandbox      = (struct sandbox *)parser->data;
	struct http_request *http_request = &sandbox->http_request;

	http_request->last_was_value = 1;
	assert(http_request->header_count <= HTTP_MAX_HEADER_COUNT);
	assert(length < HTTP_MAX_HEADER_VALUE_LENGTH);

	/* it is from the sandbox's request_response_data, should persist. */
	http_request->headers[http_request->header_count - 1].value = (char *)at;

	return 0;
}

/**
 * http-parser callback called when header parsing is complete
 * Just sets the HTTP Request's header_end flag to true
 * @param parser
 */
int
http_parser_settings_on_header_end(http_parser *parser)
{
	struct sandbox *     sandbox      = (struct sandbox *)parser->data;
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
 */
int
http_parser_settings_on_body(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *     sandbox      = (struct sandbox *)parser->data;
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
 * @returns 0
 */
int
http_parser_settings_on_msg_end(http_parser *parser)
{
	struct sandbox *     sandbox      = (struct sandbox *)parser->data;
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
static inline void
http_parser_settings_register_callbacks(http_parser_settings *settings)
{
	settings->on_url              = http_parser_settings_on_url;
	settings->on_message_begin    = http_parser_settings_on_message_begin;
	settings->on_header_field     = http_parser_settings_on_header_field;
	settings->on_header_value     = http_parser_settings_on_header_value;
	settings->on_headers_complete = http_parser_settings_on_header_end;
	settings->on_body             = http_parser_settings_on_body;
	settings->on_message_complete = http_parser_settings_on_msg_end;
}

/**
 * This is really the only function that should have to be called to setup this structure
 */
void
http_parser_settings_initialize()
{
	http_parser_settings_init(&runtime_http_parser_settings);
	http_parser_settings_register_callbacks(&runtime_http_parser_settings);
}

http_parser_settings *
http_parser_settings_get()
{
	return &runtime_http_parser_settings;
}
