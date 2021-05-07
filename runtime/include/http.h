#pragma once

#define HTTP_MAX_HEADER_COUNT        16
#define HTTP_MAX_HEADER_LENGTH       32
#define HTTP_MAX_HEADER_VALUE_LENGTH 64

#define HTTP_RESPONSE_200_OK                    "HTTP/1.1 200 OK\r\n"
#define HTTP_RESPONSE_503_SERVICE_UNAVAILABLE   "HTTP/1.1 503 Service Unavailable\r\n\r\n"
#define HTTP_RESPONSE_400_BAD_REQUEST           "HTTP/1.1 400 Bad Request\r\n\r\n"
#define HTTP_RESPONSE_CONTENT_LENGTH            "Content-Length: "
#define HTTP_RESPONSE_CONTENT_LENGTH_TERMINATOR "\r\n\r\n" /* content body follows this */
#define HTTP_RESPONSE_CONTENT_TYPE              "Content-Type: "
#define HTTP_RESPONSE_CONTENT_TYPE_PLAIN        "text/plain"
#define HTTP_RESPONSE_CONTENT_TYPE_TERMINATOR   " \r\n"
