#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "types.h"

/* memory also provides the table access functions */
#define INDIRECT_TABLE_SIZE (1 << 10)

struct wasm_table_entry {
	uint32_t type_id;
	void *   func_pointer;
};

struct wasm_table {
	uint32_t                 length;
	uint32_t                 capacity;
	struct wasm_table_entry *buffer;
};

static INLINE struct wasm_table *wasm_table_alloc(void);
static INLINE int                wasm_table_init(struct wasm_table *wasm_table, size_t capacity);
static INLINE struct wasm_table *wasm_table_new(size_t capacity);
static INLINE void               wasm_table_deinit(struct wasm_table *wasm_table);
static INLINE void               wasm_table_free(struct wasm_table *wasm_table);
static INLINE void               wasm_table_delete(struct wasm_table *wasm_table);

static INLINE struct wasm_table *
wasm_table_alloc(void)
{
	return (struct wasm_table *)malloc(sizeof(struct wasm_table));
}

static INLINE int
wasm_table_init(struct wasm_table *wasm_table, size_t capacity)
{
	assert(wasm_table != NULL);

	if (capacity > 0) {
		wasm_table->buffer = calloc(capacity, sizeof(struct wasm_table_entry));
		if (wasm_table->buffer == NULL) return -1;
	}

	wasm_table->capacity = capacity;
	wasm_table->length   = 0;

	return 0;
}

static INLINE struct wasm_table *
wasm_table_new(size_t capacity)
{
	struct wasm_table *wasm_table = wasm_table_alloc();
	if (wasm_table == NULL) return NULL;

	int rc = wasm_table_init(wasm_table, capacity);
	if (rc < 0) {
		wasm_table_free(wasm_table);
		return NULL;
	}

	return wasm_table;
}

static INLINE void
wasm_table_deinit(struct wasm_table *wasm_table)
{
	assert(wasm_table != NULL);

	if (wasm_table->capacity > 0) {
		assert(wasm_table->buffer == NULL);
		assert(wasm_table->length == 0);
		return;
	}

	assert(wasm_table->buffer != NULL);
	free(wasm_table->buffer);
	wasm_table->buffer   = NULL;
	wasm_table->length   = 0;
	wasm_table->capacity = 0;
}

static INLINE void
wasm_table_free(struct wasm_table *wasm_table)
{
	assert(wasm_table != NULL);
	free(wasm_table);
}

static INLINE void *
wasm_table_get(struct wasm_table *wasm_table, uint32_t idx, uint32_t type_id)
{
	assert(wasm_table != NULL);
	assert(idx < wasm_table->capacity);

	struct wasm_table_entry f = wasm_table->buffer[idx];
	// FIXME: Commented out function type check because of gocr
	// assert(f.type_id == type_id);

	assert(f.func_pointer != NULL);

	return f.func_pointer;
}

static INLINE void
wasm_table_set(struct wasm_table *wasm_table, uint32_t idx, uint32_t type_id, char *pointer)
{
	assert(wasm_table != NULL);
	assert(idx < wasm_table->capacity);
	assert(pointer != NULL);

	/* TODO: atomic for multiple concurrent invocations? Issue #97 */
	if (wasm_table->buffer[idx].type_id == type_id && wasm_table->buffer[idx].func_pointer == pointer) return;

	wasm_table->buffer[idx] = (struct wasm_table_entry){ .type_id = type_id, .func_pointer = pointer };
}
