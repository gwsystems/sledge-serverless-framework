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
	"Content-Length: %lu\r\n"  \
	"\r\n"

/* The sum of format specifier characters in the template above */
#define HTTP_RESPONSE_200_TEMPLATE_FORMAT_SPECIFIER_LENGTH 5

/**
 * Calculates the number of bytes of the HTTP response containing the passed header values
 * @return total size in bytes
 */
static inline size_t
http_response_200_size(const char *content_type, ssize_t content_length)
{
	size_t size = 0;
	size += strlen(HTTP_RESPONSE_200_TEMPLATE) - HTTP_RESPONSE_200_TEMPLATE_FORMAT_SPECIFIER_LENGTH;
	size += strlen(content_type);

	while (content_length > 0) {
		content_length /= 10;
		size++;
	}

	return size;
}
