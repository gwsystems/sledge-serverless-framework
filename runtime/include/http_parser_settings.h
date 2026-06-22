#pragma once

#include "llhttp.h"

extern llhttp_settings_t runtime_http_parser_settings;

void http_parser_settings_initialize(void);

static inline llhttp_settings_t *
http_parser_settings_get()
{
	return &runtime_http_parser_settings;
}
