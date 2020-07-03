#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

__attribute__((noreturn)) void
panic(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	exit(EXIT_FAILURE);
}