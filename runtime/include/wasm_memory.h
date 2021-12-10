#pragma once

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "ps_list.h"
#include "types.h" /* PAGE_SIZE */
#include "wasm_types.h"

#define WASM_MEMORY_MAX           (size_t) UINT32_MAX + 1
#define WASM_MEMORY_SIZE_TO_ALLOC ((size_t)WASM_MEMORY_MAX + /* guard page */ PAGE_SIZE)

struct wasm_memory {
	struct ps_list list;     /* Linked List Node used for object pool */
	size_t         size;     /* Initial Size in bytes */
	size_t         capacity; /* Size backed by actual pages */
	size_t         max;      /* Soft cap in bytes. Defaults to 4GB */
	uint8_t *      buffer;
};

static INLINE struct wasm_memory *wasm_memory_alloc(void);
static INLINE int                 wasm_memory_init(struct wasm_memory *self, size_t initial, size_t max);
static INLINE struct wasm_memory *wasm_memory_new(size_t initial, size_t max);
static INLINE void                wasm_memory_deinit(struct wasm_memory *self);
static INLINE void                wasm_memory_free(struct wasm_memory *self);
static INLINE void                wasm_memory_delete(struct wasm_memory *self);


static INLINE struct wasm_memory *
wasm_memory_alloc(void)
{
	return malloc(sizeof(struct wasm_memory));
}

static INLINE int
wasm_memory_init(struct wasm_memory *self, size_t initial, size_t max)
{
	assert(self != NULL);

	/* We assume WASI modules, which are required to declare and export a linear memory with a non-zero size to
	 * allow a standard lib to initialize. Technically, a WebAssembly module that exports pure functions may not use
	 * a linear memory */
	assert(initial > 0);
	assert(initial <= (size_t)UINT32_MAX + 1);
	assert(max > 0);
	assert(max <= (size_t)UINT32_MAX + 1);

	/* Allocate buffer of contiguous virtual addresses for full wasm32 linear memory and guard page */
	self->buffer = mmap(NULL, WASM_MEMORY_SIZE_TO_ALLOC, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (self->buffer == MAP_FAILED) return -1;

	/* Set the initial bytes to read / write */
	int rc = mprotect(self->buffer, initial, PROT_READ | PROT_WRITE);
	if (rc != 0) {
		munmap(self->buffer, WASM_MEMORY_SIZE_TO_ALLOC);
		return -1;
	}

	ps_list_init_d(self);
	self->size     = initial;
	self->capacity = initial;
	self->max      = max;

	return 0;
}

static INLINE struct wasm_memory *
wasm_memory_new(size_t initial, size_t max)
{
	struct wasm_memory *self = wasm_memory_alloc();
	if (self == NULL) return self;

	int rc = wasm_memory_init(self, initial, max);
	if (rc < 0) {
		assert(0);
		wasm_memory_free(self);
		return NULL;
	}

	return self;
}

static INLINE void
wasm_memory_deinit(struct wasm_memory *self)
{
	assert(self != NULL);
	assert(self->buffer != NULL);

	munmap(self->buffer, WASM_MEMORY_SIZE_TO_ALLOC);
	self->buffer   = NULL;
	self->size     = 0;
	self->capacity = 0;
	self->max      = 0;
}

static INLINE void
wasm_memory_free(struct wasm_memory *self)
{
	assert(self != NULL);
	/* Assume prior deinitialization so we don't leak buffers */
	assert(self->buffer == NULL);

	free(self);
}

static INLINE void
wasm_memory_delete(struct wasm_memory *self)
{
	assert(self != NULL);

	wasm_memory_deinit(self);
	wasm_memory_free(self);
}

static INLINE void
wasm_memory_wipe(struct wasm_memory *self)
{
	memset(self->buffer, 0, self->size);
}

static INLINE void
wasm_memory_reinit(struct wasm_memory *self, size_t initial)
{
	wasm_memory_wipe(self);
	self->size = initial;
}

static INLINE int
wasm_memory_expand(struct wasm_memory *self, size_t size_to_expand)
{
	size_t target_size = self->size + size_to_expand;
	if (unlikely(target_size > self->max)) {
		fprintf(stderr, "wasm_memory_expand - Out of Memory!. %lu out of %lu\n", self->size, self->max);
		return -1;
	}

	/* If recycling a wasm_memory from an object pool, a previous execution may have already expanded to or what
	 * beyond what we need. The capacity represents the "high water mark" of previous executions. If the desired
	 * size is less than this "high water mark," we just need to update size for accounting purposes. Otherwise, we
	 * need to actually issue an mprotect syscall. The goal of these optimizations is to reduce mmap and demand
	 * paging overhead for repeated instantiations of a WebAssembly module. */
	if (target_size > self->capacity) {
		int rc = mprotect(self->buffer, target_size, PROT_READ | PROT_WRITE);
		if (rc != 0) {
			perror("wasm_memory_expand mprotect");
			return -1;
		}

		self->capacity = target_size;
	}

	self->size = target_size;
	return 0;
}

static INLINE void
wasm_memory_set_size(struct wasm_memory *self, size_t size)
{
	self->size = size;
}

static INLINE size_t
wasm_memory_get_size(struct wasm_memory *self)
{
	return self->size;
}

static INLINE void
wasm_memory_initialize_region(struct wasm_memory *self, uint32_t offset, uint32_t region_size, uint8_t region[])
{
	assert((size_t)offset + region_size <= self->size);
	memcpy(&self->buffer[offset], region, region_size);
}

/* NOTE: These wasm_memory functions require pointer dereferencing. For this reason, they are not directly by wasm32
 * instructions. These functions are intended to be used by the runtime to interacts with linear memories. */

/**
 * Translates WASM offsets into runtime VM pointers
 * @param offset an offset into the WebAssembly linear memory
 * @param bounds_check the size of the thing we are pointing to
 * @return void pointer to something in WebAssembly linear memory
 */
static INLINE void *
wasm_memory_get_ptr_void(struct wasm_memory *self, uint32_t offset, uint32_t size)
{
	assert(offset + size <= self->size);
	return (void *)&self->buffer[offset];
}

/**
 * Get an ASCII character from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return char at the offset
 */
static INLINE char
wasm_memory_get_char(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(char) <= self->size);
	return *(char *)&self->buffer[offset];
}

/**
 * Get an float from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return float at the offset
 */
static INLINE float
wasm_memory_get_f32(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(float) <= self->size);
	return *(float *)&self->buffer[offset];
}

/**
 * Get a double from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return double at the offset
 */
static INLINE double
wasm_memory_get_f64(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(double) <= self->size);
	return *(double *)&self->buffer[offset];
}

/**
 * Get a int8_t from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int8_t at the offset
 */
static INLINE int8_t
wasm_memory_get_i8(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(int8_t) <= self->size);
	return *(int8_t *)&self->buffer[offset];
}

/**
 * Get a int16_t from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int16_t at the offset
 */
static INLINE int16_t
wasm_memory_get_i16(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(int16_t) <= self->size);
	return *(int16_t *)&self->buffer[offset];
}

/**
 * Get a int32_t from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int32_t at the offset
 */
static INLINE int32_t
wasm_memory_get_i32(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(int32_t) <= self->size);
	return *(int32_t *)&self->buffer[offset];
}

/**
 * Get a int32_t from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int32_t at the offset
 */
static INLINE int64_t
wasm_memory_get_i64(struct wasm_memory *self, uint32_t offset)
{
	assert(offset + sizeof(int64_t) <= self->size);
	return *(int64_t *)&self->buffer[offset];
}

static INLINE uint32_t
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
static INLINE char *
wasm_memory_get_string(struct wasm_memory *self, uint32_t offset, uint32_t size)
{
	assert(offset + (sizeof(char) * size) <= self->size);

	if (strnlen((const char *)&self->buffer[offset], size) < size) {
		return (char *)&self->buffer[offset];
	} else {
		return NULL;
	}
}

/**
 * Write a float to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return float at the offset
 */
static INLINE void
wasm_memory_set_f32(struct wasm_memory *self, uint32_t offset, float value)
{
	assert(offset + sizeof(float) <= self->size);
	*(float *)&self->buffer[offset] = value;
}

/**
 * Write a double to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return double at the offset
 */
static INLINE void
wasm_memory_set_f64(struct wasm_memory *self, uint32_t offset, double value)
{
	assert(offset + sizeof(double) <= self->size);
	*(double *)&self->buffer[offset] = value;
}

/**
 * Write a int8_t to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int8_t at the offset
 */
static INLINE void
wasm_memory_set_i8(struct wasm_memory *self, uint32_t offset, int8_t value)
{
	assert(offset + sizeof(int8_t) <= self->size);
	*(int8_t *)&self->buffer[offset] = value;
}

/**
 * Write a int16_t to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int16_t at the offset
 */
static INLINE void
wasm_memory_set_i16(struct wasm_memory *self, uint32_t offset, int16_t value)
{
	assert(offset + sizeof(int16_t) <= self->size);
	*(int16_t *)&self->buffer[offset] = value;
}

/**
 * Write a int32_t to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int32_t at the offset
 */
static INLINE void
wasm_memory_set_i32(struct wasm_memory *self, uint32_t offset, int32_t value)
{
	assert(offset + sizeof(int32_t) <= self->size);
	*(int32_t *)&self->buffer[offset] = value;
}

/**
 * Write a int64_t to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int64_t at the offset
 */
static INLINE void
wasm_memory_set_i64(struct wasm_memory *self, uint64_t offset, int64_t value)
{
	assert(offset + sizeof(int64_t) <= self->size);
	*(int64_t *)&self->buffer[offset] = value;
}
