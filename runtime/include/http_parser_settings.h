#pragma once

#include "http_parser.h"

extern http_parser_settings runtime_http_parser_settings;

void http_parser_settings_initialize(void);

static inline http_parser_settings *
http_parser_settings_get()
{
	return &runtime_http_parser_settings;
}
