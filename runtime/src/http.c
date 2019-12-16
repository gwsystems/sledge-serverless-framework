#include <http.h>
#include <sandbox.h>
#include <uv.h>

http_parser_settings settings;

static inline int
http_on_msg_begin(http_parser *parser)
{
	struct http_request *r = parser->data;

	r->message_begin = 1;
	r->last_was_value = 1; //should always start with a header..
	return 0;
}

static inline int
http_on_msg_end(http_parser *parser)
{
	struct http_request *r = parser->data;

	r->message_end = 1;
	return 0;
}

static inline int
http_on_header_end(http_parser *parser)
{
	struct http_request *r = parser->data;

	r->header_end = 1;
	return 0;
}

static inline int
http_on_url(http_parser* parser, const char *at, size_t length)
{
	struct sandbox *s = sandbox_current();
	struct http_request *r = parser->data;

	assert(strncmp(s->mod->name, (at + 1), length - 1) == 0);

	return 0;
}

static inline int
http_on_header_field(http_parser* parser, const char *at, size_t length)
{
	struct http_request *r = parser->data;

	if (r->last_was_value) r->nheaders ++;
	assert(r->nheaders <= HTTP_HEADERS_MAX);
	assert(length < HTTP_HEADER_MAXSZ);

	r->last_was_value = 0;
	r->headers[r->nheaders - 1].key = (char *)at; //it is from the sandbox's req_resp_data, should persist.

	return 0;
}

static inline int
http_on_header_value(http_parser* parser, const char *at, size_t length)
{
	struct http_request *r = parser->data;

	r->last_was_value = 1;
	assert(r->nheaders <= HTTP_HEADERS_MAX);
	assert(length < HTTP_HEADERVAL_MAXSZ);

	r->headers[r->nheaders - 1].val = (char *)at; //it is from the sandbox's req_resp_data, should persist.

        return 0;
}

static inline int
http_on_body(http_parser* parser, const char *at, size_t length)
{
        struct http_request *r = parser->data;
	struct sandbox *c = sandbox_current();

        assert(length <= c->mod->max_req_sz);
	r->body = (char *)at;
        r->bodylen = length;

        return 0;
}

int
http_request_body_get(char **b)
{
	struct sandbox *s = sandbox_current();
	struct http_request *r = &s->rqi;

	*b = r->body;
	return r->bodylen;
}

int
http_response_header_set(char *key, int len)
{
	// by now, req_resp_data should only be containing response!
	struct sandbox *c = sandbox_current();
	struct http_response *r = &c->rsi;

	assert(r->nheaders < HTTP_HEADERS_MAX);
	r->nheaders++;
	r->headers[r->nheaders-1].hdr = key;
	r->headers[r->nheaders-1].len = len;

	return 0;
}

int http_response_body_set(char *body, int len)
{
	struct sandbox *c = sandbox_current();
	struct http_response *r = &c->rsi;

	assert(len < c->mod->max_resp_sz);
	r->body = body;
	r->bodylen = len;

	return 0;
}

int http_response_status_set(char *status, int len)
{
	struct sandbox *c = sandbox_current();
	struct http_response *r = &c->rsi;

	r->status = status;
	r->stlen = len;

	return 0;
}

int http_response_uv(void)
{
	struct sandbox *c = sandbox_current();
	struct http_response *r = &c->rsi;

	int nb = 0;

	r->bufs[nb] = uv_buf_init(r->status, r->stlen);
	nb++;
	for (int i = 0; i < r->nheaders; i++) {
		r->bufs[nb] = uv_buf_init(r->headers[i].hdr, r->headers[i].len);
		nb++;
	}

	if (r->body) {
		r->bufs[nb] = uv_buf_init(r->body, r->bodylen);
		nb++;
		r->bufs[nb] = uv_buf_init(r->status + r->stlen - 2, 2); //for crlf
		nb++;
	}
	
	return nb;
}

int
http_request_parse(void)
{
	struct sandbox *s = sandbox_current();
	http_parser_execute(&s->hp, &settings, s->req_resp_data, s->rr_data_len);
	return 0;
}

void
http_init(void)
{
	http_parser_settings_init(&settings);
	settings.on_url          = http_on_url;
	settings.on_header_field = http_on_header_field;
	settings.on_header_value = http_on_header_value;
	settings.on_body         = http_on_body;
	settings.on_headers_complete = http_on_header_end;
	settings.on_message_begin    = http_on_msg_begin;
	settings.on_message_complete = http_on_msg_end;
}
