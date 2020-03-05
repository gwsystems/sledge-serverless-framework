#ifndef SRFT_HTTP_API_H
#define SRFT_HTTP_API_H

#include <http.h>
int http_request_body_get_sb(struct sandbox *sandbox, char **body);
int http_request_parse_sb(struct sandbox *sandbox, size_t l);

int http_response_header_set_sb(struct sandbox *sandbox, char *h, int length);
int http_response_body_set_sb(struct sandbox *sandbox, char *body, int length);
int http_response_status_set_sb(struct sandbox *sandbox, char *status, int length);
int http_response_vector_sb(struct sandbox *sandbox);

void http_init(void);

/**
 * Gets the request of the body
 * @param body pointer of pointer that we want to set to the http_request's body
 * @returns the length of the http_request's body
 **/
static inline int
http_request_body_get(char **body)
{
	return http_request_body_get_sb(sandbox_current(), body);
}

/**
 * Set an HTTP Response Header on the current sandbox
 * @param header string of the header that we want to set
 * @param length the length of the header string
 * @returns 0 (abends program in case of error)
 **/
static inline int
http_response_header_set(char *header, int length)
{
	return http_response_header_set_sb(sandbox_current(), header, length);
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
	return http_response_body_set_sb(sandbox_current(), body, length);
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
	return http_response_status_set_sb(sandbox_current(), status, length);
}

/**
 * ??? on the current sandbox
 * @returns ???
 **/
static inline int
http_response_vector(void)
{
	return http_response_vector_sb(sandbox_current());
}

/**
 * ???
 * @param length ????
 * @returns 0
 **/
static inline int
http_request_parse(size_t length)
{
	return http_request_parse_sb(sandbox_current(), length);
}

#endif /* SRFT_HTTP_API_H */
