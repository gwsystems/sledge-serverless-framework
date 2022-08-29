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

#define HTTP_RESPONSE_CONTENT_TYPE      "Content-Type: %s\r\n"
#define HTTP_RESPONSE_CONTENT_LENGTH    "Content-Length: %lu\r\n"
#define HTTP_RESPONSE_TERMINATOR        "\r\n"
#define HTTP_RESPONSE_TERMINATOR_LENGTH 2

#define HTTP_RESPONSE_200_OK  \
	"HTTP/1.1 200 OK\r\n" \
	"Server: SLEdge\r\n"  \
	"Connection: close\r\n"
#define HTTP_RESPONSE_200_OK_LENGTH 52

#define HTTP_RESPONSE_400_BAD_REQUEST  \
	"HTTP/1.1 400 Bad Request\r\n" \
	"Server: SLEdge\r\n"           \
	"Connection: close\r\n"
#define HTTP_RESPONSE_400_BAD_REQUEST_LENGTH 61

#define HTTP_RESPONSE_404_NOT_FOUND  \
	"HTTP/1.1 404 Not Found\r\n" \
	"Server: SLEdge\r\n"         \
	"Connection: close\r\n"
#define HTTP_RESPONSE_404_NOT_FOUND_LENGTH 59

#define HTTP_RESPONSE_413_PAYLOAD_TOO_LARGE  \
	"HTTP/1.1 413 Payload Too Large\r\n" \
	"Server: SLEdge\r\n"                 \
	"Connection: close\r\n"
#define HTTP_RESPONSE_413_PAYLOAD_TOO_LARGE_LENGTH 67

#define HTTP_RESPONSE_429_TOO_MANY_REQUESTS  \
	"HTTP/1.1 429 Too Many Requests\r\n" \
	"Server: SLEdge\r\n"                 \
	"Connection: close\r\n"
#define HTTP_RESPONSE_429_TOO_MANY_REQUESTS_LENGTH 67

#define HTTP_RESPONSE_500_INTERNAL_SERVER_ERROR  \
	"HTTP/1.1 500 Internal Server Error\r\n" \
	"Server: SLEdge\r\n"                     \
	"Connection: close\r\n"
#define HTTP_RESPONSE_500_INTERNAL_SERVER_ERROR_LENGTH 71

#define HTTP_RESPONSE_503_SERVICE_UNAVAILABLE  \
	"HTTP/1.1 503 Service Unavailable\r\n" \
	"Server: SLEdge\r\n"                   \
	"Connection: close\r\n"
#define HTTP_RESPONSE_503_SERVICE_UNAVAILABLE_LENGTH 69

static inline const char *
http_header_build(int status_code)
{
	switch (status_code) {
	case 200:
		return HTTP_RESPONSE_200_OK;
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
		return HTTP_RESPONSE_400_BAD_REQUEST_LENGTH;
	case 404:
		return HTTP_RESPONSE_404_NOT_FOUND_LENGTH;
	case 413:
		return HTTP_RESPONSE_413_PAYLOAD_TOO_LARGE_LENGTH;
	case 429:
		return HTTP_RESPONSE_429_TOO_MANY_REQUESTS_LENGTH;
	case 500:
		return HTTP_RESPONSE_500_INTERNAL_SERVER_ERROR_LENGTH;
	case 503:
		return HTTP_RESPONSE_503_SERVICE_UNAVAILABLE_LENGTH;
	default:
		panic("%d is not a valid status code\n", status_code);
	}
}
