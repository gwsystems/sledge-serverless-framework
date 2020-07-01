#pragma once

#include "http_parser.h"

void                  http_parser_settings_initialize(void);
http_parser_settings *http_parser_settings_get();
