#pragma once

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "current_sandbox.h"
#include "http.h"
#include "http_total.h"
#include "likely.h"
#include "sandbox_types.h"
#include "scheduler.h"
#include "panic.h"

/**
 * Sends Response Back to Client
 * @return RC. -1 on Failure
 */
static inline int
sandbox_send_response(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	/*
	 * At this point the HTTP Request has filled the buffer up to request_length, after which
	 * the STDOUT of the sandbox has been appended. We assume that our HTTP Response header is
	 * smaller than the HTTP Request header, which allows us to use memmove once without copying
	 * to an intermediate buffer.
	 */
	memset(sandbox->request_response_data, 0, sandbox->request_length);

	/*
	 * We use this cursor to keep track of our position in the buffer and later assert that we
	 * haven't overwritten body data.
	 */
	size_t response_cursor = 0;

	/* Append 200 OK */
	strncpy(sandbox->request_response_data, HTTP_RESPONSE_200_OK, strlen(HTTP_RESPONSE_200_OK));
	response_cursor += strlen(HTTP_RESPONSE_200_OK);

	/* Content Type */
	strncpy(sandbox->request_response_data + response_cursor, HTTP_RESPONSE_CONTENT_TYPE,
	        strlen(HTTP_RESPONSE_CONTENT_TYPE));
	response_cursor += strlen(HTTP_RESPONSE_CONTENT_TYPE);

	/* Custom content type if provided, text/plain by default */
	if (strlen(sandbox->module->response_content_type) <= 0) {
		strncpy(sandbox->request_response_data + response_cursor, HTTP_RESPONSE_CONTENT_TYPE_PLAIN,
		        strlen(HTTP_RESPONSE_CONTENT_TYPE_PLAIN));
		response_cursor += strlen(HTTP_RESPONSE_CONTENT_TYPE_PLAIN);
	} else {
		strncpy(sandbox->request_response_data + response_cursor, sandbox->module->response_content_type,
		        strlen(sandbox->module->response_content_type));
		response_cursor += strlen(sandbox->module->response_content_type);
	}

	strncpy(sandbox->request_response_data + response_cursor, HTTP_RESPONSE_CONTENT_TYPE_TERMINATOR,
	        strlen(HTTP_RESPONSE_CONTENT_TYPE_TERMINATOR));
	response_cursor += strlen(HTTP_RESPONSE_CONTENT_TYPE_TERMINATOR);

	/* Content Length */
	strncpy(sandbox->request_response_data + response_cursor, HTTP_RESPONSE_CONTENT_LENGTH,
	        strlen(HTTP_RESPONSE_CONTENT_LENGTH));
	response_cursor += strlen(HTTP_RESPONSE_CONTENT_LENGTH);

	size_t body_size = sandbox->request_response_data_length - sandbox->request_length;

	char len[10] = { 0 };
	sprintf(len, "%zu", body_size);
	strncpy(sandbox->request_response_data + response_cursor, len, strlen(len));
	response_cursor += strlen(len);

	strncpy(sandbox->request_response_data + response_cursor, HTTP_RESPONSE_CONTENT_LENGTH_TERMINATOR,
	        strlen(HTTP_RESPONSE_CONTENT_LENGTH_TERMINATOR));
	response_cursor += strlen(HTTP_RESPONSE_CONTENT_LENGTH_TERMINATOR);

	/*
	 * Assumption: Our response header is smaller than the request header, so we do not overwrite
	 * actual data that the program appended to the HTTP Request. If proves to be a bad assumption,
	 * we have to copy the STDOUT string to a temporary buffer before writing the header
	 */
	if (unlikely(response_cursor >= sandbox->request_length)) {
		panic("Response Cursor: %zd is less that Request Length: %zd\n", response_cursor,
		      sandbox->request_length);
	}

	/* Move the Sandbox's Data after the HTTP Response Data */
	memmove(sandbox->request_response_data + response_cursor,
	        sandbox->request_response_data + sandbox->request_length, body_size);
	response_cursor += body_size;

	/* Capture Timekeeping data for end-to-end latency */
	uint64_t end_time   = __getcycles();
	sandbox->total_time = end_time - sandbox->request_arrival_timestamp;

	int rc;
	int sent = 0;
	while (sent < response_cursor) {
		rc = write(sandbox->client_socket_descriptor, &sandbox->request_response_data[sent],
		           response_cursor - sent);
		if (rc < 0) {
			if (errno == EAGAIN)
				scheduler_block();
			else {
				perror("write");
				return -1;
			}
		}

		sent += rc;
	}

	http_total_increment_2xx();

	return 0;
}
