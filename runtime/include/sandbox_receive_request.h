#pragma once

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "current_sandbox.h"
#include "debuglog.h"
#include "http_parser.h"
#include "http_request.h"
#include "http_parser_settings.h"
#include "likely.h"
#include "sandbox_types.h"
#include "scheduler.h"

/**
 * Receive and Parse the Request for the current sandbox
 * @return 0 if message parsing complete, -1 on error, -2 if buffers run out of space
 */
static inline int
sandbox_receive_request(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox->module->max_request_size > 0);
	assert(sandbox->request.length == 0);

	int rc = 0;

	while (!sandbox->http_request.message_end) {
		/* Read from the Socket */

		/* Structured to closely follow usage example at https://github.com/nodejs/http-parser */
		http_parser *               parser   = &sandbox->http_parser;
		const http_parser_settings *settings = http_parser_settings_get();

		if (sandbox->module->max_request_size <= sandbox->request.length) {
			debuglog("Sandbox %lu: Ran out of Request Buffer before message end\n", sandbox->id);
			goto err_nobufs;
		}

		ssize_t bytes_received = recv(sandbox->client_socket_descriptor,
		                              &sandbox->request.base[sandbox->request.length],
		                              sandbox->module->max_request_size - sandbox->request.length, 0);

		if (bytes_received == -1) {
			if (errno == EAGAIN) {
				current_sandbox_sleep();
				continue;
			} else {
				debuglog("Error reading socket %d - %s\n", sandbox->client_socket_descriptor,
				         strerror(errno));
				goto err;
			}
		}

		/* If we received an EOF before we were able to parse a complete HTTP header, request is malformed */
		if (bytes_received == 0 && !sandbox->http_request.message_end) {
			char client_address_text[INET6_ADDRSTRLEN] = {};
			if (unlikely(inet_ntop(AF_INET, &sandbox->client_address, client_address_text, INET6_ADDRSTRLEN)
			             == NULL)) {
				debuglog("Failed to log client_address: %s", strerror(errno));
			}

			debuglog("Sandbox %lu: recv returned 0 before a complete request was received\n", sandbox->id);
			debuglog("Socket: %d. Address: %s\n", sandbox->client_socket_descriptor, client_address_text);
			http_request_print(&sandbox->http_request);
			goto err;
		}

#ifdef LOG_HTTP_PARSER
		debuglog("Sandbox: %lu http_parser_execute(%p, %p, %p, %zu\n)", sandbox->id, parser, settings, buf,
		         bytes_received);
#endif
		size_t bytes_parsed = http_parser_execute(parser, settings,
		                                          &sandbox->request.base[sandbox->request.length],
		                                          bytes_received);

		if (bytes_parsed != bytes_received) {
			debuglog("Error: %s, Description: %s\n",
			         http_errno_name((enum http_errno)sandbox->http_parser.http_errno),
			         http_errno_description((enum http_errno)sandbox->http_parser.http_errno));
			debuglog("Length Parsed %zu, Length Read %zu\n", bytes_parsed, bytes_received);
			debuglog("Error parsing socket %d\n", sandbox->client_socket_descriptor);
			goto err;
		}

		sandbox->request.length += bytes_parsed;
	}

	rc = 0;
done:
	return rc;
err_nobufs:
	rc = -2;
	goto done;
err:
	rc = -1;
	goto done;
}
