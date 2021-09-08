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
	/* Assumption: The HTTP Request Buffer immediately precedes the HTTP Response Buffer,
	 * meaning that when we prepend, we are overwritting the tail of the HTTP request buffer */
	assert(sandbox->request.base + sandbox->module->max_request_size == sandbox->response.base);

	int rc;

	/* Determine values to template into our HTTP response */
	ssize_t response_body_size = sandbox->response.length;
	char    content_length[20] = { 0 };
	sprintf(content_length, "%zu", response_body_size);
	char *module_content_type = sandbox->module->response_content_type;
	char *content_type        = strlen(module_content_type) > 0 ? module_content_type : "text/plain";

	/* Prepend HTTP Response Headers */
	size_t response_header_size = http_response_200_size(content_type, content_length);
	char * response_header      = sandbox->response.base - response_header_size;
	rc                          = http_response_200(response_header, content_type, content_length);
	if (rc < 0) goto err;

	/* Capture Timekeeping data for end-to-end latency */
	uint64_t end_time   = __getcycles();
	sandbox->total_time = end_time - sandbox->timestamp_of.request_arrival;

	/* Send HTTP Response */
	int    sent          = 0;
	size_t response_size = response_header_size + response_body_size;
	while (sent < response_size) {
		rc = write(sandbox->client_socket_descriptor, response_header, response_size - sent);
		if (rc < 0) {
			if (errno == EAGAIN)
				scheduler_block();
			else {
				perror("write");
				goto err;
			}
		}
		sent += rc;
	}

	http_total_increment_2xx();
	rc = 0;

done:
	return rc;
err:
	rc = -1;
	goto done;
}
