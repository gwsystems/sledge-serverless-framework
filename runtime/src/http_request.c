#include <stdio.h>

#include "http_request.h"

/***************************************************
 * General HTTP Request Functions                  *
 **************************************************/

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
