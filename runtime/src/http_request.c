#include <stdio.h>

#include "http_request.h"

/***************************************************
 * General HTTP Request Functions                  *
 **************************************************/

/**
 * Gets the HTTP Request body from a Sandbox
 * @param sandbox the sandbox we want the body from
 * @param body pointer that we'll assign to the http_request body
 * @returns the length of the http_request's body
 */
int
http_request_get_body(struct http_request *http_request, char **body)
{
	*body = http_request->body;
	return http_request->body_length;
}

void
http_request_print(struct http_request *self)
{
	printf("Header Count %d\n", self->header_count);
	printf("Header Content:\n");
	for (int i = 0; i < self->header_count; i++) {
		for (int j = 0; j < self->headers[i].key_length; j++) { putchar(self->headers[i].key[j]); }
		putchar(':');
		for (int j = 0; j < self->headers[i].value_length; j++) { putchar(self->headers[i].value[j]); }
		putchar('\n');
	}
	printf("Body Length %d\n", self->body_length);
	printf("Body Read Length %d\n", self->body_read_length);
}
