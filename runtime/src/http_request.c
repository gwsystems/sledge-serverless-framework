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
