#pragma once

#include <string.h>

#include "http_total.h"
#include "panic.h"

#define HTTP_MAX_HEADER_COUNT        32
#define HTTP_MAX_HEADER_LENGTH       64
#define HTTP_MAX_HEADER_VALUE_LENGTH 256
#define HTTP_MAX_FULL_URL_LENGTH     256

#define HTTP_MAX_QUERY_PARAM_COUNT  16
#define HTTP_MAX_QUERY_PARAM_LENGTH 32

#define HTTP_RESPONSE_200_TEMPLATE \
	"HTTP/1.1 200 OK\r\n"      \
	"Server: SLEdge\r\n"       \
	"Connection: close\r\n"    \
	"Content-Type: %s\r\n"     \
	"Content-Length: %lu\r\n"  \
	"\r\n"

/* The sum of format specifier characters in the template above */
#define HTTP_RESPONSE_200_TEMPLATE_FORMAT_SPECIFIER_LENGTH 5

#define HTTP_RESPONSE_400_BAD_REQUEST  \
	"HTTP/1.1 400 Bad Request\r\n" \
	"Server: SLEdge\r\n"           \
	"Connection: close\r\n"        \
	"\r\n"

#define HTTP_RESPONSE_404_NOT_FOUND  \
	"HTTP/1.1 404 Not Found\r\n" \
	"Server: SLEdge\r\n"         \
	"Connection: close\r\n"      \
	"\r\n"

#define HTTP_RESPONSE_413_PAYLOAD_TOO_LARGE  \
	"HTTP/1.1 413 Payload Too Large\r\n" \
	"Server: SLEdge\r\n"                 \
	"Connection: close\r\n"              \
	"\r\n"

#define HTTP_RESPONSE_429_TOO_MANY_REQUESTS  \
	"HTTP/1.1 429 Too Many Requests\r\n" \
	"Server: SLEdge\r\n"                 \
	"Connection: close\r\n"              \
	"\r\n"

#define HTTP_RESPONSE_500_INTERNAL_SERVER_ERROR  \
	"HTTP/1.1 500 Internal Server Error\r\n" \
	"Server: SLEdge\r\n"                     \
	"Connection: close\r\n"                  \
	"\r\n"

#define HTTP_RESPONSE_503_SERVICE_UNAVAILABLE  \
	"HTTP/1.1 503 Service Unavailable\r\n" \
	"Server: SLEdge\r\n"                   \
	"Connection: close\r\n"                \
	"\r\n"

static inline const char *
http_header_build(int status_code)
{
	switch (status_code) {
	case 400:
		return HTTP_RESPONSE_400_BAD_REQUEST;
	case 404:
		return HTTP_RESPONSE_404_NOT_FOUND;
	case 413:
		return HTTP_RESPONSE_413_PAYLOAD_TOO_LARGE;
	case 429:
		return HTTP_RESPONSE_429_TOO_MANY_REQUESTS;
	case 500:
		return HTTP_RESPONSE_500_INTERNAL_SERVER_ERROR;
	case 503:
		return HTTP_RESPONSE_503_SERVICE_UNAVAILABLE;
	default:
		panic("%d is not a valid status code\n", status_code);
	}
}

static inline size_t
http_header_len(int status_code)
{
	switch (status_code) {
	case 400:
		return strlen(HTTP_RESPONSE_400_BAD_REQUEST);
	case 404:
		return strlen(HTTP_RESPONSE_404_NOT_FOUND);
	case 413:
		return strlen(HTTP_RESPONSE_413_PAYLOAD_TOO_LARGE);
	case 429:
		return strlen(HTTP_RESPONSE_429_TOO_MANY_REQUESTS);
	case 500:
		return strlen(HTTP_RESPONSE_500_INTERNAL_SERVER_ERROR);
	case 503:
		return strlen(HTTP_RESPONSE_503_SERVICE_UNAVAILABLE);
	default:
		panic("%d is not a valid status code\n", status_code);
	}
}
