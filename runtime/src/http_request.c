#include <stdio.h>

#include "http_request.h"

/***************************************************
 * General HTTP Request Functions                  *
 **************************************************/

void
http_request_print(struct http_request *http_request)
{
	printf("Header Count %d\n", http_request->header_count);
	printf("Header Content:\n");
	for (int i = 0; i < http_request->header_count; i++) {
		for (int j = 0; j < http_request->headers[i].key_length; j++) {
			putchar(http_request->headers[i].key[j]);
		}
		putchar(':');
		for (int j = 0; j < http_request->headers[i].value_length; j++) {
			putchar(http_request->headers[i].value[j]);
		}
		putchar('\n');
	}
	printf("Body Length %d\n", http_request->body_length);
	printf("Body Read Length %d\n", http_request->body_read_length);
}
