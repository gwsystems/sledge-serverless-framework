#pragma once

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "debuglog.h"
#include "types.h" /* PAGE_SIZE */
#include "wasm_types.h"

#define WASM_LINEAR_MEMORY_MAX (size_t) UINT32_MAX + 1

struct wasm_linear_memory {
	size_t  size; /* Initial Size in bytes */
	size_t  max;  /* Soft cap in bytes. Defaults to 4GB */
	uint8_t data[];
};

static inline struct wasm_linear_memory *
wasm_linear_memory_allocate(size_t initial, size_t max)
{
	assert(initial > 0);
	assert(initial <= (size_t)UINT32_MAX + 1);
	assert(max > 0);
	assert(max <= (size_t)UINT32_MAX + 1);

	char *                     error_message = NULL;
	int                        rc            = 0;
	struct wasm_linear_memory *self          = NULL;

	/* Allocate contiguous virtual addresses and map to fault */
	size_t size_to_alloc = sizeof(struct wasm_linear_memory) + WASM_LINEAR_MEMORY_MAX + /* guard page */ PAGE_SIZE;

	void *addr = mmap(NULL, size_to_alloc, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		debuglog("wasm_linear_memory_allocate - allocation failed, (size: %lu) %s\n", size_to_alloc,
		         strerror(errno));
		return self;
	}

	/* Set the struct and initial pages to read / write */
	size_t size_to_read_write = sizeof(struct wasm_linear_memory) + initial;

	void *addr_rw = mmap(addr, size_to_read_write, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
	                     -1, 0);
	if (addr_rw == MAP_FAILED) {
		perror("wasm_linear_memory_allocate - prot r/w failed");
		munmap(addr, size_to_alloc);
		return self;
	}


	self       = (struct wasm_linear_memory *)addr_rw;
	self->max  = max;
	self->size = initial;
	return self;
}

static inline void
wasm_linear_memory_free(struct wasm_linear_memory *self)
{
	size_t size_to_free = sizeof(struct wasm_linear_memory) + self->max + /* guard page */ PAGE_SIZE;
	munmap(self, size_to_free);
}


static inline int
wasm_linear_memory_expand(struct wasm_linear_memory *self, size_t size_to_expand)
{
	if (unlikely(self->size + size_to_expand >= self->max)) {
		debuglog("wasm_linear_memory_expand - Out of Memory!.\n");
		return -1;
	}

	void *temp = mmap(self, sizeof(struct wasm_linear_memory) + self->size + size_to_expand, PROT_READ | PROT_WRITE,
	                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	if (temp == NULL) {
		perror("wasm_linear_memory_expand mmap");
		return -1;
	}

	/* Assumption: We are not actually resizing our wasm_linear_memory capacity. We are just adjusting the R/W rules
	 * within a preallocated wasm_linear_memory of size max */
	assert(self == temp);

	self->size += size_to_expand;
	return 0;
}

static inline int
wasm_linear_memory_resize(struct wasm_linear_memory *self, size_t target_size)
{
	if (unlikely(target_size >= self->max)) {
		debuglog("wasm_linear_memory_expand - Out of Memory!. %lu out of %lu\n", self->size, self->max);
		return -1;
	}

	void *temp = mmap(self, sizeof(struct wasm_linear_memory) + target_size, PROT_READ | PROT_WRITE,
	                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	if (temp == NULL) {
		perror("wasm_linear_memory_resize mmap");
		return -1;
	}

	assert(self == temp);

	/* Assumption: We are not actually resizing our wasm_linear_memory capacity. We are just adjusting the R/W rules
	 * within a preallocated wasm_linear_memory of size max */
	self->size = target_size;
	return 0;
}

/**
 * Translates WASM offsets into runtime VM pointers
 * @param offset an offset into the WebAssembly linear memory
 * @param bounds_check the size of the thing we are pointing to
 * @return void pointer to something in WebAssembly linear memory
 */
static inline void *
wasm_linear_memory_get_ptr_void(struct wasm_linear_memory *self, uint32_t offset, uint32_t size)
{
	assert(offset + size <= self->size);
	return (void *)&self->data[offset];
}

/**
 * Get an ASCII character from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return char at the offset
 */
static inline char
wasm_linear_memory_get_char(struct wasm_linear_memory *self, uint32_t offset)
{
	assert(offset + sizeof(char) <= self->size);
	return (char)self->data[offset];
}

/**
 * Get a null-terminated String from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @param size the maximum expected length in characters
 * @return pointer to the string or NULL if max_length is reached without finding null-terminator
 */
static inline char *
wasm_linear_memory_get_string(struct wasm_linear_memory *self, uint32_t offset, uint32_t size)
{
	assert(offset + (sizeof(char) * size) <= self->size);

	for (uint32_t i = 0; i < size; i++) {
		if (self->data[offset + i] == '\0') return (char *)&self->data[offset];
	}
	return NULL;
}
