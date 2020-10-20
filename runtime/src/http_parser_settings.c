#include "debuglog.h"
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

	assert(!sandbox->http_request.message_end);
	assert(!sandbox->http_request.header_end);

#ifdef LOG_HTTP_PARSER
	debuglog("sandbox: %lu, length: %zu, Content \"%.*s\"\n", sandbox->id, length, (int)length, at);
	assert(strncmp(sandbox->module->name, (at + 1), length - 1) == 0);
#endif

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

	assert(!sandbox->http_request.message_end);
	assert(!sandbox->http_request.header_end);

#ifdef LOG_HTTP_PARSER
	debuglog("sandbox: %lu\n", sandbox->id);
#endif

	http_request->message_begin  = true;
	http_request->last_was_value = true; /* should always start with a header */

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

#ifdef LOG_HTTP_PARSER
	debuglog("sandbox: %lu, length: %zu, Content \"%.*s\"\n", sandbox->id, length, (int)length, at);
#endif

	assert(!sandbox->http_request.message_end);
	assert(!sandbox->http_request.header_end);

	if (http_request->last_was_value == false) {
		/* Previous key continues */
		assert(http_request->header_count > 0);
		if (unlikely(http_request->headers[http_request->header_count].key_length + length
		             > HTTP_MAX_HEADER_LENGTH)) {
			return -1;
		}
		http_request->headers[http_request->header_count].key_length += length;
		return 0;
	} else {
		/* Start of new key */
		if (unlikely(http_request->header_count >= HTTP_MAX_HEADER_COUNT)) return -1;
		if (unlikely(length > HTTP_MAX_HEADER_LENGTH)) return -1;
		http_request->header_count++;
		http_request->headers[http_request->header_count - 1].key        = (char *)at;
		http_request->headers[http_request->header_count - 1].key_length = length;
		http_request->last_was_value                                     = false;
	}

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


#ifdef LOG_HTTP_PARSER
	debuglog("sandbox: %lu, length: %zu, Content \"%.*s\"\n", sandbox->id, length, (int)length, at);
#endif

	assert(!sandbox->http_request.message_end);
	assert(!sandbox->http_request.header_end);

	if (!http_request->last_was_value) {
		if (unlikely(length >= HTTP_MAX_HEADER_VALUE_LENGTH)) return -1;
		http_request->headers[http_request->header_count - 1].value        = (char *)at;
		http_request->headers[http_request->header_count - 1].value_length = length;
	} else {
		assert(http_request->headers[http_request->header_count - 1].value_length > 0);
		http_request->headers[http_request->header_count - 1].value_length += length;
	}

	http_request->last_was_value = true;
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

	assert(!sandbox->http_request.message_end);
	assert(!sandbox->http_request.header_end);

#ifdef LOG_HTTP_PARSER
	debuglog("sandbox: %lu\n", sandbox->id);
#endif

	http_request->header_end = true;
	return 0;
}

const size_t http_methods_len               = 8;
const char * http_methods[http_methods_len] = { "OPTIONS", "GET", "HEAD", "POST", "PUT", "DELETE", "TRACE", "CONNECT" };

/**
 * http-parser callback called for HTTP Bodies
 * Assigns the parsed data to the http_request body of the sandbox struct
 * Presumably, this might only be part of the body
 * @param parser
 * @param at - start address of body
 * @param length - length of body
 * @returns 0
 */
int
http_parser_settings_on_body(http_parser *parser, const char *at, size_t length)
{
	struct sandbox *     sandbox      = (struct sandbox *)parser->data;
	struct http_request *http_request = &sandbox->http_request;

	assert(sandbox->http_request.header_end);
	assert(!sandbox->http_request.message_end);


	/* Assumption: We should never exceed the buffer we're reusing */
	assert(http_request->body_length + length <= sandbox->module->max_request_size);


	if (!http_request->body) {
#ifdef LOG_HTTP_PARSER
		debuglog("Setting start of body!\n");
#endif
		/* If this is the first invocation of the callback, just set */
		http_request->body        = (char *)at;
		http_request->body_length = length;
	} else {
#ifdef LOG_HTTP_PARSER
		debuglog("Appending to existing body!\n");
#endif
		http_request->body_length += length;
	}

#ifdef LOG_HTTP_PARSER
	int capped_len = length > 1000 ? 1000 : length;
	debuglog("sandbox: %lu, length: %zu, Content(up to 1000 chars) \"%.*s\"\n", sandbox->id, length,
	         (int)capped_len, at);
#endif

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

	assert(sandbox->http_request.header_end);
	assert(!sandbox->http_request.message_end);

#ifdef LOG_HTTP_PARSER
	debuglog("sandbox: %lu\n", sandbox->id);
#endif

	http_request->message_end = true;

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
