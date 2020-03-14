#include <http.h>
#include <sandbox.h>
#include <uv.h>
#include <http_api.h>

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
sandbox__get_http_request_body(struct sandbox *sandbox, char **body)
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
sandbox__set_http_response_header(struct sandbox *sandbox, char *header, int length)
{
	// by now, request_response_data should only be containing response!
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
sandbox__set_http_response_body(struct sandbox *sandbox, char *body, int length)
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
sandbox__set_http_response_status(struct sandbox *sandbox, char *status, int length)
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
sandbox__vectorize_http_response(struct sandbox *sandbox)
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

/**
 * Run the http-parser on the sandbox's request_response_data using the configured settings global
 * @param sandbox the sandbox containing the req_resp data that we want to parse
 * @param length The size of the request_response_data that we want to parse
 * @returns 0
 * 
 * Globals: global__http_parser_settings
 **/
int
sandbox__parse_http_request(struct sandbox *sandbox, size_t length)
{
	// Why is our start address sandbox->request_response_data + sandbox->request_response_data_length?
	// it's like a cursor to keep track of what we've read so far
	http_parser_execute(&sandbox->http_parser, &global__http_parser_settings, sandbox->request_response_data + sandbox->request_response_data_length, length);
	return 0;
}
