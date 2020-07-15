#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

__attribute__((noreturn, format(printf, 1, 2))) static inline void
panic(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	assert(0);
}
