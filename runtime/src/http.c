#include <http.h>
#include <sandbox.h>
#include <uv.h>
#include <http_api.h>

http_parser_settings settings;

static inline int
http_on_msg_begin(http_parser *parser)
{
#ifndef STANDALONE
	struct sandbox *s = parser->data;
	struct http_request *r = &s->rqi;

	r->message_begin = 1;
	r->last_was_value = 1; //should always start with a header..
#endif
	return 0;
}

static inline int
http_on_msg_end(http_parser *parser)
{
#ifndef STANDALONE
	struct sandbox *s = parser->data;
	struct http_request *r = &s->rqi;

	r->message_end = 1;
#endif
	return 0;
}

static inline int
http_on_header_end(http_parser *parser)
{
#ifndef STANDALONE
	struct sandbox *s = parser->data;
	struct http_request *r = &s->rqi;

	r->header_end = 1;
#endif
	return 0;
}

static inline int
http_on_url(http_parser* parser, const char *at, size_t length)
{
#ifndef STANDALONE
	struct sandbox *s = parser->data;
	struct http_request *r = &s->rqi;

	assert(strncmp(s->mod->name, (at + 1), length - 1) == 0);
#endif
	return 0;
}

static inline int
http_on_header_field(http_parser* parser, const char *at, size_t length)
{
	struct sandbox *s = parser->data;
	struct http_request *r = &s->rqi;

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
	struct sandbox *s = parser->data;
	struct http_request *r = &s->rqi;

	r->last_was_value = 1;
	assert(r->nheaders <= HTTP_HEADERS_MAX);
	assert(length < HTTP_HEADERVAL_MAXSZ);

	r->headers[r->nheaders - 1].val = (char *)at; //it is from the sandbox's req_resp_data, should persist.

        return 0;
}

static inline int
http_on_body(http_parser* parser, const char *at, size_t length)
{
#ifndef STANDALONE
	struct sandbox *s = parser->data;
	struct http_request *r = &s->rqi;

        assert(r->bodylen + length <= s->mod->max_req_sz);
	if (!r->body) r->body = (char *)at;
	else assert(r->body + r->bodylen == at);

        r->bodylen += length;
#endif

        return 0;
}

int
http_request_body_get_sb(struct sandbox *s, char **b)
{
#ifndef STANDALONE
	struct http_request *r = &s->rqi;

	*b = r->body;
	return r->bodylen;
#else
	return 0;
#endif
}

int
http_response_header_set_sb(struct sandbox *c, char *key, int len)
{
#ifndef STANDALONE
	// by now, req_resp_data should only be containing response!
	struct http_response *r = &c->rsi;

	assert(r->nheaders < HTTP_HEADERS_MAX);
	r->nheaders++;
	r->headers[r->nheaders-1].hdr = key;
	r->headers[r->nheaders-1].len = len;
#endif

	return 0;
}

int
http_response_body_set_sb(struct sandbox *c, char *body, int len)
{
#ifndef STANDALONE
	struct http_response *r = &c->rsi;

	assert(len <= c->mod->max_resp_sz);
	r->body = body;
	r->bodylen = len;
#endif

	return 0;
}

int
http_response_status_set_sb(struct sandbox *c, char *status, int len)
{
#ifndef STANDALONE
	struct http_response *r = &c->rsi;

	r->status = status;
	r->stlen = len;
#endif

	return 0;
}

int
http_response_uv_sb(struct sandbox *c)
{
	int nb = 0;
#ifndef STANDALONE
	struct http_response *r = &c->rsi;


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
#endif
	
	return nb;
}

int
http_request_parse_sb(struct sandbox *s, size_t l)
{
#ifndef STANDALONE
	http_parser_execute(&s->hp, &settings, s->req_resp_data + s->rr_data_len, l);
#endif
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
