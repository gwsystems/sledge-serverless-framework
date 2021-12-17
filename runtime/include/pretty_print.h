#pragma once

#include <stdio.h>
#include <stdarg.h>

#define PRETTY_PRINT_COLOR_CODE_RED   "\033[1;31m"
#define PRETTY_COLOR_CODE_GREEN       "\033[0;32m"
#define PRETTY_PRINT_COLOR_CODE_RESET "\033[0m"
#define PRETTY_PRINT_GREEN_ENABLED    PRETTY_COLOR_CODE_GREEN "Enabled" PRETTY_PRINT_COLOR_CODE_RESET
#define PRETTY_PRINT_RED_DISABLED     PRETTY_PRINT_COLOR_CODE_RED "Disabled" PRETTY_PRINT_COLOR_CODE_RESET
#define PRETTY_PRINT_KEY_LEN          30


static inline void
pretty_print_key(char *heading)
{
	printf("\t%-*s", PRETTY_PRINT_KEY_LEN, heading);
}

static inline void
pretty_print_key_value(char *key, char *fmt, ...)
{
	va_list ap;
	pretty_print_key(key);

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static inline void
pretty_print_key_enabled(char *key)
{
	pretty_print_key_value(key, "%s\n", PRETTY_PRINT_GREEN_ENABLED);
}

static inline void
pretty_print_key_disabled(char *key)
{
	pretty_print_key_value(key, "%s\n", PRETTY_PRINT_RED_DISABLED);
}
