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
current_sandbox_send_response()
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);

	int     rc;
	ssize_t sent = 0;

	/* Determine values to template into our HTTP response */
	ssize_t     response_body_size  = sandbox->response.length;
	char *      module_content_type = sandbox->module->response_content_type;
	const char *content_type        = strlen(module_content_type) > 0 ? module_content_type : "text/plain";

	/* Capture Timekeeping data for end-to-end latency */
	uint64_t end_time   = __getcycles();
	sandbox->total_time = end_time - sandbox->timestamp_of.request_arrival;

	/* Send HTTP Response Headers */
	ssize_t response_size = http_response_200_size(content_type, response_body_size);
	char    header_buffer[response_size + 1];
	rc = sprintf(header_buffer, HTTP_RESPONSE_200_TEMPLATE, content_type, response_body_size);
	if (rc <= 0) {
		perror("sprintf");
		goto err;
	}

	while (sent < response_size) {
		rc = write(sandbox->client_socket_descriptor, &header_buffer[sent], response_size - sent);
		if (rc < 0) {
			if (errno == EAGAIN)
				current_sandbox_sleep();
			else {
				perror("write");
				goto err;
			}
		}
		sent += rc;
	}

	/* Send HTTP Response Body */
	sent          = 0;
	response_size = response_body_size;
	while (sent < response_size) {
		rc = write(sandbox->client_socket_descriptor, &sandbox->response.base[sent], response_size - sent);
		if (rc < 0) {
			if (errno == EAGAIN)
				current_sandbox_sleep();
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
	debuglog("Error sending to client: %s", strerror(errno));
	rc = -1;
	goto done;
}
