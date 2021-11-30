#pragma once

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "debuglog.h"
#include "types.h" /* PAGE_SIZE */

struct buffer {
	uint32_t size;
	uint64_t max;
	uint8_t  data[];
};

static inline struct buffer *
buffer_allocate(size_t initial, size_t max)
{
	char *         error_message = NULL;
	int            rc            = 0;
	struct buffer *self          = NULL;
	assert(initial > 0);
	assert(max > 0);

	size_t size_to_alloc = sizeof(struct buffer) + max + /* guard page */ PAGE_SIZE;
	// assert(round_up_to_page(size_to_alloc) == size_to_alloc);

	void *addr = mmap(NULL, size_to_alloc, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		debuglog("buffer_allocate - allocation failed, (size: %lu) %s\n", size_to_alloc, strerror(errno));
		return self;
	}

	/* Set as read / write */
	size_t size_to_read_write = sizeof(struct buffer) + initial;

	void *addr_rw = mmap(addr, size_to_read_write, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
	                     -1, 0);
	if (addr_rw == MAP_FAILED) {
		perror("buffer_allocate - prot r/w failed");
		munmap(addr, size_to_alloc);
		return self;
	}


	self       = (struct buffer *)addr_rw;
	self->max  = max;
	self->size = initial;
	return self;
}

static inline void
buffer_free(struct buffer *self)
{
	size_t size_to_free = sizeof(struct buffer) + self->max + /* guard page */ PAGE_SIZE;
	munmap(self, size_to_free);
}


static inline int
buffer_expand(struct buffer *self, size_t size_to_expand)
{
	if (unlikely(self->size + size_to_expand >= self->max)) {
		debuglog("buffer_expand - Out of Memory!. %u out of %lu\n", self->size, self->max);
		return -1;
	}

	void *temp = mmap(self, sizeof(struct buffer) + self->size + size_to_expand, PROT_READ | PROT_WRITE,
	                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	if (temp == NULL) {
		perror("buffer_expand mmap");
		return -1;
	}

	/* Assumption: We are not actually resizing our buffer capacity. We are just adjusting the R/W rules within a
	 * preallocated buffer of size max */
	assert(self == temp);

	self->size += size_to_expand;
	return 0;
}

static inline int
buffer_resize(struct buffer *self, size_t target_size)
{
	if (unlikely(target_size >= self->max)) {
		debuglog("buffer_expand - Out of Memory!. %u out of %lu\n", self->size, self->max);
		return -1;
	}

	void *temp = mmap(self, sizeof(struct buffer) + target_size, PROT_READ | PROT_WRITE,
	                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	if (temp == NULL) {
		perror("buffer_resize mmap");
		return -1;
	}

	assert(self == temp);

	/* Assumption: We are not actually resizing our buffer capacity. We are just adjusting the R/W rules within a
	 * preallocated buffer of size max */
	self->size = target_size;
	return 0;
}
