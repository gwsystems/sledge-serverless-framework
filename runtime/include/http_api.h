#ifndef SRFT_HTTP_API_H
#define SRFT_HTTP_API_H

#include <http.h>

/***************************************************
 * General HTTP Request Functions                  *
 **************************************************/
int sandbox__get_http_request_body(struct sandbox *sandbox, char **body);

/**
 * Gets the HTTP Request body from the current sandbox
 * @param body pointer that we'll assign to the http_request body
 * @returns the length of the http_request's body
 **/
static inline int
current_sandbox__get_http_request_body(char **body)
{
	return sandbox__get_http_request_body(get_current_sandbox(), body);
}

/***************************************************
 * General HTTP Response Functions                 *
 **************************************************/
int sandbox__set_http_response_header(struct sandbox *sandbox, char *h, int length);
int sandbox__set_http_response_body(struct sandbox *sandbox, char *body, int length);
int sandbox__set_http_response_status(struct sandbox *sandbox, char *status, int length);
int sandbox__vectorize_http_response(struct sandbox *sandbox);

/**
 * Set an HTTP Response Header on the current sandbox
 * @param header string of the header that we want to set
 * @param length the length of the header string
 * @returns 0 (abends program in case of error)
 **/
static inline int
current_sandbox__set_http_response_header(char *header, int length)
{
	return sandbox__set_http_response_header(get_current_sandbox(), header, length);
}

/**
 * Set an HTTP Response Body on the current sandbox
 * @param body string of the body that we want to set
 * @param length the length of the body string
 * @returns 0 (abends program in case of error)
 **/
static inline int
current_sandbox__set_http_response_body(char *body, int length)
{
	return sandbox__set_http_response_body(get_current_sandbox(), body, length);
}

/**
 * Set an HTTP Response Status on the current sandbox
 * @param status string of the status we want to set
 * @param length the length of the status
 * @returns 0 (abends program in case of error)
 **/
static inline int
current_sandbox__set_http_response_status(char *status, int length)
{
	return sandbox__set_http_response_status(get_current_sandbox(), status, length);
}

/**
 * Encode the current sandbox's HTTP Response as an array of buffers
 * @returns the number of buffers used to store the HTTP Response
 **/
static inline int
current_sandbox__vectorize_http_response(void)
{
	return sandbox__vectorize_http_response(get_current_sandbox());
}

/***********************************************************************
 * http-parser Setup and Excute                                        *
 **********************************************************************/
// void global__http_parser_settings__initialize_and_register_callbacks(void);

extern http_parser_settings global__http_parser_settings;

int sandbox__parse_http_request(struct sandbox *sandbox, size_t l);

/**
 * Parse the current sandbox's request_response_data up to length
 * @param length
 * @returns 0
 **/
static inline int
current_sandbox__parse_http_request(size_t length)
{
	return sandbox__parse_http_request(get_current_sandbox(), length);
}

#endif /* SRFT_HTTP_API_H */
