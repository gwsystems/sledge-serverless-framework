#pragma once

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "types.h" /* PAGE_SIZE */
#include "wasm_types.h"

#define WASM_MEMORY_MAX (size_t) UINT32_MAX + 1

struct wasm_memory {
	size_t  size;     /* Initial Size in bytes */
	size_t  capacity; /* Size backed by actual pages */
	size_t  max;      /* Soft cap in bytes. Defaults to 4GB */
	uint8_t data[];
};

static inline struct wasm_memory *
wasm_memory_allocate(size_t initial, size_t max)
{
	assert(initial > 0);
	assert(initial <= (size_t)UINT32_MAX + 1);
	assert(max > 0);
	assert(max <= (size_t)UINT32_MAX + 1);

	/* Allocate contiguous virtual addresses for struct, full linear memory, and guard page */
	size_t size_to_alloc = sizeof(struct wasm_memory) + WASM_MEMORY_MAX + /* guard page */ PAGE_SIZE;
	void * temp          = mmap(NULL, size_to_alloc, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (temp == MAP_FAILED) {
		fprintf(stderr, "wasm_memory_allocate - allocation failed, (size: %lu) %s\n", size_to_alloc,
		        strerror(errno));
		return NULL;
	}
	struct wasm_memory *self = (struct wasm_memory *)temp;

	/* Set the struct and initial pages to read / write */
	size_t size_to_read_write = sizeof(struct wasm_memory) + initial;

	int rc = mprotect(self, size_to_read_write, PROT_READ | PROT_WRITE);
	if (rc != 0) {
		perror("wasm_memory_allocate - prot r/w failed");
		munmap(self, size_to_alloc);
		assert(0);
		return NULL;
	}

	self->size     = initial;
	self->capacity = initial;
	self->max      = max;
	return self;
}

static inline void
wasm_memory_free(struct wasm_memory *self)
{
	size_t size_to_free = sizeof(struct wasm_memory) + WASM_MEMORY_MAX + /* guard page */ PAGE_SIZE;
	munmap(self, size_to_free);
}

static inline void
wasm_memory_wipe(struct wasm_memory *self)
{
	memset(self->data, 0, self->size);
}

static inline int
wasm_memory_expand(struct wasm_memory *self, size_t size_to_expand)
{
	size_t target_size = self->size + size_to_expand;
	if (unlikely(target_size > self->max)) {
		fprintf(stderr, "wasm_memory_expand - Out of Memory!. %lu out of %lu\n", self->size, self->max);
		return -1;
	}

	if (target_size > self->capacity) {
		int rc = mprotect(self, sizeof(struct wasm_memory) + target_size, PROT_READ | PROT_WRITE);
		if (rc != 0) {
			perror("wasm_memory_expand mprotect");
			return -1;
		}

		self->capacity = target_size;
	}

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
wasm_memory_get_ptr_void(struct wasm_memory *self, uint32_t offset, uint32_t size)
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
wasm_memory_get_char(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(char) <= self->size);
	return *(char *)&self->data[offset];
}

/**
 * Get an float from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return float at the offset
 */
static inline float
wasm_memory_get_f32(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(float) <= self->size);
	return *(float *)&self->data[offset];
}

/**
 * Get a double from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return double at the offset
 */
static inline double
wasm_memory_get_f64(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(double) <= self->size);
	return *(double *)&self->data[offset];
}

/**
 * Get a int8_t from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int8_t at the offset
 */
static inline int8_t
wasm_memory_get_i8(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(int8_t) <= self->size);
	return *(int8_t *)&self->data[offset];
}

/**
 * Get a int16_t from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int16_t at the offset
 */
static inline int16_t
wasm_memory_get_i16(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(int16_t) <= self->size);
	return *(int16_t *)&self->data[offset];
}

/**
 * Get a int32_t from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int32_t at the offset
 */
static inline int32_t
wasm_memory_get_i32(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(int32_t) <= self->size);
	return *(int32_t *)&self->data[offset];
}

/**
 * Get a int32_t from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int32_t at the offset
 */
static inline int64_t
wasm_memory_get_i64(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(int64_t) <= self->size);
	return *(int64_t *)&self->data[offset];
}

static inline uint32_t
wasm_memory_get_page_count(struct wasm_memory *self)
{
	return (uint32_t)(self->size / WASM_PAGE_SIZE);
}

/**
 * Get a null-terminated String from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @param size the maximum expected length in characters
 * @return pointer to the string or NULL if max_length is reached without finding null-terminator
 */
static inline char *
wasm_memory_get_string(struct wasm_memory *self, uint32_t offset, uint32_t size)
{
	assert(offset + (sizeof(char) * size) <= self->size);

	for (uint32_t i = 0; i < size; i++) {
		if (self->data[offset + i] == '\0') return (char *)&self->data[offset];
	}
	return NULL;
}

/**
 * Write a float to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return float at the offset
 */
static inline void
wasm_memory_set_f32(struct wasm_memory *self, uint32_t offset, float value)
{
	assert(offset + sizeof(float) <= self->size);
	*(float *)&self->data[offset] = value;
}

/**
 * Write a double to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return double at the offset
 */
static inline void
wasm_memory_set_f64(struct wasm_memory *self, uint32_t offset, double value)
{
	assert(offset + sizeof(double) <= self->size);
	*(double *)&self->data[offset] = value;
}

/**
 * Write a int8_t to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int8_t at the offset
 */
static inline void
wasm_memory_set_i8(struct wasm_memory *self, uint32_t offset, int8_t value)
{
	assert(offset + sizeof(int8_t) <= self->size);
	*(int8_t *)&self->data[offset] = value;
}

/**
 * Write a int16_t to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int16_t at the offset
 */
static inline void
wasm_memory_set_i16(struct wasm_memory *self, uint32_t offset, int16_t value)
{
	assert(offset + sizeof(int16_t) <= self->size);
	*(int16_t *)&self->data[offset] = value;
}

/**
 * Write a int32_t to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int32_t at the offset
 */
static inline void
wasm_memory_set_i32(struct wasm_memory *self, uint32_t offset, int32_t value)
{
	assert(offset + sizeof(int32_t) <= self->size);
	*(int32_t *)&self->data[offset] = value;
}

/**
 * Write a int64_t to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int64_t at the offset
 */
static inline void
wasm_memory_set_i64(struct wasm_memory *self, uint64_t offset, int64_t value)
{
	assert(offset + sizeof(int64_t) <= self->size);
	*(int64_t *)&self->data[offset] = value;
}

static inline void
wasm_memory_set_size(struct wasm_memory *self, size_t size)
{
	self->size = size;
}

static inline size_t
wasm_memory_get_size(struct wasm_memory *self)
{
	return self->size;
}

static inline void
wasm_memory_initialize_region(struct wasm_memory *self, uint32_t offset, uint32_t region_size, uint8_t region[])
{
	assert((size_t)offset + region_size <= self->size);
	memcpy(&self->data[offset], region, region_size);
}
