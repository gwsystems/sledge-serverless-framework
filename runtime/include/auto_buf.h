#pragma once

#include "likely.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

struct auto_buf {
	FILE  *handle;
	char  *data;
	size_t size;
};

static inline int
auto_buf_init(struct auto_buf *buf)
{
	FILE *res = open_memstream(&buf->data, &buf->size);
	if (res == NULL) return errno;

	buf->handle = res;
	return 0;
}

static inline int
auto_buf_flush(struct auto_buf *buf)
{
	return fflush(buf->handle);
}

static inline void
auto_buf_deinit(struct auto_buf *buf)
{
	if (likely(buf->handle != NULL)) {
		fclose(buf->handle);
		buf->handle = NULL;
	}

	if (likely(buf->data != NULL)) {
		free(buf->data);
		buf->data = NULL;
	}

	buf->size = 0;
}
