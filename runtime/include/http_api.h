#ifndef SRFT_HTTP_API_H
#define SRFT_HTTP_API_H

#include <http.h>

/***************************************************
 * General HTTP Request Functions                  *
 **************************************************/
int http_request_body_get_sb(struct sandbox *sandbox, char **body);

/**
 * Gets the HTTP Request body from the current sandbox
 * @param body pointer that we'll assign to the http_request body
 * @returns the length of the http_request's body
 **/
static inline int
http_request_body_get(char **body)
{
	return http_request_body_get_sb(get_current_sandbox(), body);
}

/***************************************************
 * General HTTP Response Functions                 *
 **************************************************/
int http_response_header_set_sb(struct sandbox *sandbox, char *h, int length);
int http_response_body_set_sb(struct sandbox *sandbox, char *body, int length);
int http_response_status_set_sb(struct sandbox *sandbox, char *status, int length);
int http_response_vector_sb(struct sandbox *sandbox);

/**
 * Set an HTTP Response Header on the current sandbox
 * @param header string of the header that we want to set
 * @param length the length of the header string
 * @returns 0 (abends program in case of error)
 **/
static inline int
http_response_header_set(char *header, int length)
{
	return http_response_header_set_sb(get_current_sandbox(), header, length);
}

/**
 * Set an HTTP Response Body on the current sandbox
 * @param body string of the body that we want to set
 * @param length the length of the body string
 * @returns 0 (abends program in case of error)
 **/
static inline int
http_response_body_set(char *body, int length)
{
	return http_response_body_set_sb(get_current_sandbox(), body, length);
}

/**
 * Set an HTTP Response Status on the current sandbox
 * @param status string of the status we want to set
 * @param length the length of the status
 * @returns 0 (abends program in case of error)
 **/
static inline int
http_response_status_set(char *status, int length)
{
	return http_response_status_set_sb(get_current_sandbox(), status, length);
}

/**
 * Encode the current sandbox's HTTP Response as an array of buffers
 * @returns the number of buffers used to store the HTTP Response
 **/
static inline int
http_response_vector(void)
{
	return http_response_vector_sb(get_current_sandbox());
}

/***********************************************************************
 * http-parser Setup and Excute                                        *
 **********************************************************************/
void http_init(void);
int http_request_parse_sb(struct sandbox *sandbox, size_t l);

/**
 * Parse the current sandbox's request_response_data up to length
 * @param length
 * @returns 0
 **/
static inline int
http_request_parse(size_t length)
{
	return http_request_parse_sb(get_current_sandbox(), length);
}

#endif /* SRFT_HTTP_API_H */
