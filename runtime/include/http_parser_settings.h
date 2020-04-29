#ifndef SRFT_HTTP_PARSER_SETTINGS_H
#define SRFT_HTTP_PARSER_SETTINGS_H

#include "http_parser.h"

void                  http_parser_settings_initialize(void);
http_parser_settings *http_parser_settings_get();

#endif /* SRFT_HTTP_PARSER_SETTINGS_H */
