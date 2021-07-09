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
 * @return 0 if message parsing complete, -1 on error
 */
static inline int
sandbox_receive_request(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox->module->max_request_size > 0);
	assert(sandbox->request_response_data_length == 0);

	int rc = 0;

	while (!sandbox->http_request.message_end) {
		/* Read from the Socket */

		/* Structured to closely follow usage example at https://github.com/nodejs/http-parser */
		http_parser *               parser   = &sandbox->http_parser;
		const http_parser_settings *settings = http_parser_settings_get();

		int    fd  = sandbox->client_socket_descriptor;
		char * buf = &sandbox->request_response_data[sandbox->request_response_data_length];
		size_t len = sandbox->module->max_request_size - sandbox->request_response_data_length;

		ssize_t recved = recv(fd, buf, len, 0);

		if (recved < 0) {
			if (errno == EAGAIN) {
				scheduler_block();
				continue;
			} else {
				/* All other errors */
				debuglog("Error reading socket %d - %s\n", sandbox->client_socket_descriptor,
				         strerror(errno));
				goto err;
			}
		}

		/* Client request is malformed */
		if (recved == 0 && !sandbox->http_request.message_end) {
			char client_address_text[INET6_ADDRSTRLEN] = {};
			if (unlikely(inet_ntop(AF_INET, &sandbox->client_address, client_address_text, INET6_ADDRSTRLEN)
			             == NULL)) {
				debuglog("Failed to log client_address: %s", strerror(errno));
			}
			uint16_t port = (((struct sockaddr_in6*)&sandbox->client_address)->sin6_port);
			debuglog("Sandbox %lu: recv returned 0 before a complete request was received\n", sandbox->id);
			debuglog("Socket: %d. Address: %s, Port: %u\n", fd, client_address_text, port);
			http_request_print(&sandbox->http_request);
			goto err;
		}

#ifdef LOG_HTTP_PARSER
		debuglog("Sandbox: %lu http_parser_execute(%p, %p, %p, %zu\n)", sandbox->id, parser, settings, buf,
		         recved);
#endif
		size_t nparsed = http_parser_execute(parser, settings, buf, recved);

		if (nparsed != recved) {
			/* TODO: Is this error  */
			debuglog("Error: %s, Description: %s\n",
			         http_errno_name((enum http_errno)sandbox->http_parser.http_errno),
			         http_errno_description((enum http_errno)sandbox->http_parser.http_errno));
			debuglog("Length Parsed %zu, Length Read %zu\n", nparsed, recved);
			debuglog("Error parsing socket %d\n", sandbox->client_socket_descriptor);
			goto err;
		}


		sandbox->request_response_data_length += nparsed;
	}

	//http_request_print(&sandbox->http_request);
	sandbox->request_length = sandbox->request_response_data_length;

	rc = 0;
done:
	return rc;
err:
	rc = -1;
	goto done;
}
