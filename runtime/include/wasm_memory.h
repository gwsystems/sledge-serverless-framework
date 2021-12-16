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
	uint8_t *      buffer;   /* Backing heap allocation. Different lifetime because realloc might move this */
};

/* Object Lifecycle Functions */
static INLINE struct wasm_memory *wasm_memory_alloc(size_t initial, size_t max);
static INLINE int                 wasm_memory_init(struct wasm_memory *wasm_memory, size_t initial, size_t max);
static INLINE void                wasm_memory_deinit(struct wasm_memory *wasm_memory);
static INLINE void                wasm_memory_free(struct wasm_memory *wasm_memory);
static INLINE void                wasm_memory_reinit(struct wasm_memory *wasm_memory, size_t initial);

/* Memory Size */
static INLINE int      wasm_memory_expand(struct wasm_memory *wasm_memory, size_t size_to_expand);
static INLINE size_t   wasm_memory_get_size(struct wasm_memory *wasm_memory);
static INLINE uint32_t wasm_memory_get_page_count(struct wasm_memory *wasm_memory);

/* Reading and writing to wasm_memory */
static INLINE void
wasm_memory_initialize_region(struct wasm_memory *wasm_memory, uint32_t offset, uint32_t region_size, uint8_t region[]);
static INLINE void *  wasm_memory_get_ptr_void(struct wasm_memory *wasm_memory, uint32_t offset, uint32_t size);
static INLINE int8_t  wasm_memory_get_i8(struct wasm_memory *wasm_memory, uint32_t offset);
static INLINE int16_t wasm_memory_get_i16(struct wasm_memory *wasm_memory, uint32_t offset);
static INLINE int32_t wasm_memory_get_i32(struct wasm_memory *wasm_memory, uint32_t offset);
static INLINE int64_t wasm_memory_get_i64(struct wasm_memory *wasm_memory, uint32_t offset);
static INLINE float   wasm_memory_get_f32(struct wasm_memory *wasm_memory, uint32_t offset);
static INLINE double  wasm_memory_get_f64(struct wasm_memory *wasm_memory, uint32_t offset);
static INLINE char    wasm_memory_get_char(struct wasm_memory *wasm_memory, uint32_t offset);
static INLINE char *  wasm_memory_get_string(struct wasm_memory *wasm_memory, uint32_t offset, uint32_t size);
static INLINE void    wasm_memory_set_i8(struct wasm_memory *wasm_memory, uint32_t offset, int8_t value);
static INLINE void    wasm_memory_set_i16(struct wasm_memory *wasm_memory, uint32_t offset, int16_t value);
static INLINE void    wasm_memory_set_i32(struct wasm_memory *wasm_memory, uint32_t offset, int32_t value);
static INLINE void    wasm_memory_set_i64(struct wasm_memory *wasm_memory, uint64_t offset, int64_t value);
static INLINE void    wasm_memory_set_f32(struct wasm_memory *wasm_memory, uint32_t offset, float value);
static INLINE void    wasm_memory_set_f64(struct wasm_memory *wasm_memory, uint32_t offset, double value);

static INLINE struct wasm_memory *
wasm_memory_alloc(size_t initial, size_t max)
{
	struct wasm_memory *wasm_memory = malloc(sizeof(struct wasm_memory));
	if (wasm_memory == NULL) return wasm_memory;

	int rc = wasm_memory_init(wasm_memory, initial, max);
	if (rc < 0) {
		assert(0);
		wasm_memory_free(wasm_memory);
		return NULL;
	}

	return wasm_memory;
}

static INLINE int
wasm_memory_init(struct wasm_memory *wasm_memory, size_t initial, size_t max)
{
	assert(wasm_memory != NULL);

	/* We assume WASI modules, which are required to declare and export a linear memory with a non-zero size to
	 * allow a standard lib to initialize. Technically, a WebAssembly module that exports pure functions may not use
	 * a linear memory */
	assert(initial > 0);
	assert(initial <= (size_t)UINT32_MAX + 1);
	assert(max > 0);
	assert(max <= (size_t)UINT32_MAX + 1);

	/* Allocate buffer of contiguous virtual addresses for full wasm32 linear memory and guard page */
	wasm_memory->buffer = mmap(NULL, WASM_MEMORY_SIZE_TO_ALLOC, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (wasm_memory->buffer == MAP_FAILED) return -1;

	/* Set the initial bytes to read / write */
	int rc = mprotect(wasm_memory->buffer, initial, PROT_READ | PROT_WRITE);
	if (rc != 0) {
		munmap(wasm_memory->buffer, WASM_MEMORY_SIZE_TO_ALLOC);
		return -1;
	}

	ps_list_init_d(wasm_memory);
	wasm_memory->size     = initial;
	wasm_memory->capacity = initial;
	wasm_memory->max      = max;

	return 0;
}

static INLINE void
wasm_memory_deinit(struct wasm_memory *wasm_memory)
{
	assert(wasm_memory != NULL);
	assert(wasm_memory->buffer != NULL);

	munmap(wasm_memory->buffer, WASM_MEMORY_SIZE_TO_ALLOC);
	wasm_memory->buffer   = NULL;
	wasm_memory->size     = 0;
	wasm_memory->capacity = 0;
	wasm_memory->max      = 0;
}

static INLINE void
wasm_memory_free(struct wasm_memory *wasm_memory)
{
	assert(wasm_memory != NULL);
	wasm_memory_deinit(wasm_memory);
	free(wasm_memory);
}

static INLINE void
wasm_memory_reinit(struct wasm_memory *wasm_memory, size_t initial)
{
	memset(wasm_memory->buffer, 0, wasm_memory->size);
	wasm_memory->size = initial;
}

static INLINE int
wasm_memory_expand(struct wasm_memory *wasm_memory, size_t size_to_expand)
{
	size_t target_size = wasm_memory->size + size_to_expand;
	if (unlikely(target_size > wasm_memory->max)) {
		fprintf(stderr, "wasm_memory_expand - Out of Memory!. %lu out of %lu\n", wasm_memory->size,
		        wasm_memory->max);
		return -1;
	}

	/* If recycling a wasm_memory from an object pool, a previous execution may have already expanded to or what
	 * beyond what we need. The capacity represents the "high water mark" of previous executions. If the desired
	 * size is less than this "high water mark," we just need to update size for accounting purposes. Otherwise, we
	 * need to actually issue an mprotect syscall. The goal of these optimizations is to reduce mmap and demand
	 * paging overhead for repeated instantiations of a WebAssembly module. */
	if (target_size > wasm_memory->capacity) {
		int rc = mprotect(wasm_memory->buffer, target_size, PROT_READ | PROT_WRITE);
		if (rc != 0) {
			perror("wasm_memory_expand mprotect");
			return -1;
		}

		wasm_memory->capacity = target_size;
	}

	wasm_memory->size = target_size;
	return 0;
}

static INLINE size_t
wasm_memory_get_size(struct wasm_memory *wasm_memory)
{
	return wasm_memory->size;
}

static INLINE void
wasm_memory_initialize_region(struct wasm_memory *wasm_memory, uint32_t offset, uint32_t region_size, uint8_t region[])
{
	assert((size_t)offset + region_size <= wasm_memory->size);
	memcpy(&wasm_memory->buffer[offset], region, region_size);
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
wasm_memory_get_ptr_void(struct wasm_memory *wasm_memory, uint32_t offset, uint32_t size)
{
	assert(offset + size <= wasm_memory->size);
	return (void *)&wasm_memory->buffer[offset];
}

/**
 * Get an ASCII character from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return char at the offset
 */
static INLINE char
wasm_memory_get_char(struct wasm_memory *wasm_memory, uint32_t offset)
{
	assert(offset + sizeof(char) <= wasm_memory->size);
	return *(char *)&wasm_memory->buffer[offset];
}

/**
 * Get an float from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return float at the offset
 */
static INLINE float
wasm_memory_get_f32(struct wasm_memory *wasm_memory, uint32_t offset)
{
	assert(offset + sizeof(float) <= wasm_memory->size);
	return *(float *)&wasm_memory->buffer[offset];
}

/**
 * Get a double from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return double at the offset
 */
static INLINE double
wasm_memory_get_f64(struct wasm_memory *wasm_memory, uint32_t offset)
{
	assert(offset + sizeof(double) <= wasm_memory->size);
	return *(double *)&wasm_memory->buffer[offset];
}

/**
 * Get a int8_t from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int8_t at the offset
 */
static INLINE int8_t
wasm_memory_get_i8(struct wasm_memory *wasm_memory, uint32_t offset)
{
	assert(offset + sizeof(int8_t) <= wasm_memory->size);
	return *(int8_t *)&wasm_memory->buffer[offset];
}

/**
 * Get a int16_t from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int16_t at the offset
 */
static INLINE int16_t
wasm_memory_get_i16(struct wasm_memory *wasm_memory, uint32_t offset)
{
	assert(offset + sizeof(int16_t) <= wasm_memory->size);
	return *(int16_t *)&wasm_memory->buffer[offset];
}

/**
 * Get a int32_t from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int32_t at the offset
 */
static INLINE int32_t
wasm_memory_get_i32(struct wasm_memory *wasm_memory, uint32_t offset)
{
	assert(offset + sizeof(int32_t) <= wasm_memory->size);
	return *(int32_t *)&wasm_memory->buffer[offset];
}

/**
 * Get a int32_t from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int32_t at the offset
 */
static INLINE int64_t
wasm_memory_get_i64(struct wasm_memory *wasm_memory, uint32_t offset)
{
	assert(offset + sizeof(int64_t) <= wasm_memory->size);
	return *(int64_t *)&wasm_memory->buffer[offset];
}

static INLINE uint32_t
wasm_memory_get_page_count(struct wasm_memory *wasm_memory)
{
	return (uint32_t)(wasm_memory->size / WASM_PAGE_SIZE);
}

/**
 * Get a null-terminated String from WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @param size the maximum expected length in characters
 * @return pointer to the string or NULL if max_length is reached without finding null-terminator
 */
static INLINE char *
wasm_memory_get_string(struct wasm_memory *wasm_memory, uint32_t offset, uint32_t size)
{
	assert(offset + (sizeof(char) * size) <= wasm_memory->size);

	if (strnlen((const char *)&wasm_memory->buffer[offset], size) < size) {
		return (char *)&wasm_memory->buffer[offset];
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
wasm_memory_set_f32(struct wasm_memory *wasm_memory, uint32_t offset, float value)
{
	assert(offset + sizeof(float) <= wasm_memory->size);
	*(float *)&wasm_memory->buffer[offset] = value;
}

/**
 * Write a double to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return double at the offset
 */
static INLINE void
wasm_memory_set_f64(struct wasm_memory *wasm_memory, uint32_t offset, double value)
{
	assert(offset + sizeof(double) <= wasm_memory->size);
	*(double *)&wasm_memory->buffer[offset] = value;
}

/**
 * Write a int8_t to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int8_t at the offset
 */
static INLINE void
wasm_memory_set_i8(struct wasm_memory *wasm_memory, uint32_t offset, int8_t value)
{
	assert(offset + sizeof(int8_t) <= wasm_memory->size);
	*(int8_t *)&wasm_memory->buffer[offset] = value;
}

/**
 * Write a int16_t to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int16_t at the offset
 */
static INLINE void
wasm_memory_set_i16(struct wasm_memory *wasm_memory, uint32_t offset, int16_t value)
{
	assert(offset + sizeof(int16_t) <= wasm_memory->size);
	*(int16_t *)&wasm_memory->buffer[offset] = value;
}

/**
 * Write a int32_t to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int32_t at the offset
 */
static INLINE void
wasm_memory_set_i32(struct wasm_memory *wasm_memory, uint32_t offset, int32_t value)
{
	assert(offset + sizeof(int32_t) <= wasm_memory->size);
	*(int32_t *)&wasm_memory->buffer[offset] = value;
}

/**
 * Write a int64_t to WebAssembly linear memory
 * @param offset an offset into the WebAssembly linear memory
 * @return int64_t at the offset
 */
static INLINE void
wasm_memory_set_i64(struct wasm_memory *wasm_memory, uint64_t offset, int64_t value)
{
	assert(offset + sizeof(int64_t) <= wasm_memory->size);
	*(int64_t *)&wasm_memory->buffer[offset] = value;
}