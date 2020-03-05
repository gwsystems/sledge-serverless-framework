#ifndef SRFT_HTTP_API_H
#define SRFT_HTTP_API_H

#include <http.h>
int http_request_body_get_sb(struct sandbox *sandbox, char **body);
int http_request_parse_sb(struct sandbox *sandbox, size_t l);

int http_response_header_set_sb(struct sandbox *sandbox, char *h, int len);
int http_response_body_set_sb(struct sandbox *sandbox, char *body, int len);
int http_response_status_set_sb(struct sandbox *sandbox, char *status, int len);
int http_response_vector_sb(struct sandbox *sandbox);

void http_init(void);

static inline int
http_request_body_get(char **b)
{
	return http_request_body_get_sb(sandbox_current(), b);
}

static inline int
http_response_header_set(char *key, int len)
{
	return http_response_header_set_sb(sandbox_current(), key, len);
}

static inline int
http_response_body_set(char *body, int len)
{
	return http_response_body_set_sb(sandbox_current(), body, len);
}

static inline int
http_response_status_set(char *status, int len)
{
	return http_response_status_set_sb(sandbox_current(), status, len);
}

static inline int
http_response_vector(void)
{
	return http_response_vector_sb(sandbox_current());
}

static inline int
http_request_parse(size_t l)
{
	return http_request_parse_sb(sandbox_current(), l);
}

#endif /* SRFT_HTTP_API_H */
