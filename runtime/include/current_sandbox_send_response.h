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
	struct vec_u8 *response = sandbox->response;
	assert(response != NULL);

	int     rc;
	ssize_t sent = 0;

	/* Determine values to template into our HTTP response */
	ssize_t     response_body_size  = response->length;
	char *      module_content_type = sandbox->module->response_content_type;
	const char *content_type        = strlen(module_content_type) > 0 ? module_content_type : "text/plain";

	/* Capture Timekeeping data for end-to-end latency */
	uint64_t end_time   = __getcycles();
	sandbox->total_time = end_time - sandbox->timestamp_of.request_arrival;

	/* Generate and send HTTP Response Headers */
	ssize_t response_header_size = http_response_200_size(content_type, response_body_size);
	char    response_header_buffer[response_header_size + 1];
	rc = http_header_200_build(response_header_buffer, content_type, response_body_size);
	if (rc <= 0) {
		perror("sprintf");
		goto err;
	}
	client_socket_send(sandbox->client_socket_descriptor, response_header_buffer, response_header_size,
	                   current_sandbox_sleep);

	/* Send HTTP Response Body */
	client_socket_send(sandbox->client_socket_descriptor, (const char *)response->buffer,
	                   response_body_size, current_sandbox_sleep);

	http_total_increment_2xx();
	rc = 0;

done:
	return rc;
err:
	debuglog("Error sending to client: %s", strerror(errno));
	rc = -1;
	goto done;
}
