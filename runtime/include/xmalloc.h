#pragma once

#include <stdlib.h>

#include "likely.h"
#include "panic.h"

static inline void *
xmalloc(size_t size)
{
	void *allocation = malloc(size);
	if (unlikely(allocation == NULL)) panic("xmalloc failed!\n");
	return allocation;
}
