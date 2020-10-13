#include <assert.h>

#include "http_response.h"

/***************************************************
 * General HTTP Response Functions                 *
 **************************************************/

/**
 * Encodes a sandbox's HTTP Response as an array of buffers
 * @param sandbox the sandbox containing the HTTP response we want to encode as buffers
 * @returns the number of buffers used to store the HTTP Response
 */
int
http_response_encode_as_vector(struct http_response *http_response)
{
	int buffer_count = 0;

	http_response->bufs[buffer_count].iov_base = http_response->status;
	http_response->bufs[buffer_count].iov_len  = http_response->status_length;
	buffer_count++;

	for (int i = 0; i < http_response->header_count; i++) {
		http_response->bufs[buffer_count].iov_base = http_response->headers[i].header;
		http_response->bufs[buffer_count].iov_len  = http_response->headers[i].length;
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

	return buffer_count;
}

/**
 * Set an HTTP Response Body on a Sandbox
 * @param sandbox the sandbox we want to set the request header on
 * @param body string of the body that we want to set
 * @param length the length of the header string
 * @returns 0 (abends program in case of error)
 */
int
http_response_set_body(struct http_response *http_response, char *body, int length)
{
	// assert(length <= sandbox->module->max_response_size);
	http_response->body        = body;
	http_response->body_length = length;

	return 0;
}

/**
 * Append a header to the HTTP Response of a Sandbox
 * @param sandbox the sandbox we want to set the request header on
 * @param header string containing the header that we want to append
 * @param length the length of the header string
 * @returns 0 (abends program in case of error)
 */
int
http_response_set_header(struct http_response *http_response, char *header, int length)
{
	assert(http_response->header_count < HTTP_MAX_HEADER_COUNT);
	http_response->header_count++;
	http_response->headers[http_response->header_count - 1].header = header;
	http_response->headers[http_response->header_count - 1].length = length;

	return 0;
}

/**
 * Set an HTTP Response Status on a Sandbox
 * @param sandbox the sandbox we want to set the request status on
 * @param status string of the status we want to set
 * @param length the length of the status
 * @returns 0 (abends program in case of error)
 */
int
http_response_set_status(struct http_response *http_response, char *status, int length)
{
	http_response->status        = status;
	http_response->status_length = length;

	return 0;
}
