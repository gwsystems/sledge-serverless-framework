#pragma once

#include <string.h>

#define HTTP_MAX_HEADER_COUNT        16
#define HTTP_MAX_HEADER_LENGTH       32
#define HTTP_MAX_HEADER_VALUE_LENGTH 64

#define HTTP_RESPONSE_400_BAD_REQUEST  \
	"HTTP/1.1 400 Bad Request\r\n" \
	"Server: SLEdge\r\n"           \
	"Connection: close\r\n"        \
	"\r\n"


#define HTTP_RESPONSE_413_PAYLOAD_TOO_LARGE  \
	"HTTP/1.1 413 Payload Too Large\r\n" \
	"Server: SLEdge\r\n"                 \
	"Connection: close\r\n"              \
	"\r\n"

#define HTTP_RESPONSE_503_SERVICE_UNAVAILABLE  \
	"HTTP/1.1 503 Service Unavailable\r\n" \
	"Server: SLEdge\r\n"                   \
	"Connection: close\r\n"                \
	"\r\n"

#define HTTP_RESPONSE_200_TEMPLATE \
	"HTTP/1.1 200 OK\r\n"      \
	"Server: SLEdge\r\n"       \
	"Connection: close\r\n"    \
	"Content-Type: %s\r\n"     \
	"Content-Length: %s\r\n"   \
	"\r\n"

/* The sum of format specifier characters in the template above */
#define HTTP_RESPONSE_200_TEMPLATE_FORMAT_SPECIFIER_LENGTH 4

/**
 * Calculates the number of bytes of the HTTP response containing the passed header values
 * @return total size in bytes
 */
static inline size_t
http_response_200_size(char *content_type, char *content_length)
{
	size_t size = 0;
	size += strlen(HTTP_RESPONSE_200_TEMPLATE) - HTTP_RESPONSE_200_TEMPLATE_FORMAT_SPECIFIER_LENGTH;
	size += strlen(content_type);
	size += strlen(content_length);
	return size;
}

/**
 * Writes the HTTP response header to the destination. This is assumed to have been sized
 * using the value returned by http_response_200_size. We have to use an intermediate buffer
 * in order to truncate off the null terminator
 * @return 0 on success, -1 otherwise
 */
static inline int
http_response_200(char *destination, char *content_type, char *content_length)
{
	size_t response_size = http_response_200_size(content_type, content_length);
	char   buffer[response_size + 1];
	int    rc = 0;
	rc        = sprintf(buffer, HTTP_RESPONSE_200_TEMPLATE, content_type, content_length);
	if (rc <= 0) goto err;
	memmove(destination, buffer, response_size);
	rc = 0;

done:
	return rc;
err:
	rc = -1;
	goto done;
}
