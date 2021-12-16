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
static INLINE int                wasm_table_init(struct wasm_table *self, size_t capacity);
static INLINE struct wasm_table *wasm_table_new(size_t capacity);
static INLINE void               wasm_table_deinit(struct wasm_table *self);
static INLINE void               wasm_table_free(struct wasm_table *self);
static INLINE void               wasm_table_delete(struct wasm_table *self);

static INLINE struct wasm_table *
wasm_table_alloc(void)
{
	return (struct wasm_table *)malloc(sizeof(struct wasm_table));
}

static INLINE int
wasm_table_init(struct wasm_table *self, size_t capacity)
{
	assert(self != NULL);

	if (capacity > 0) {
		self->buffer = calloc(capacity, sizeof(struct wasm_table_entry));
		if (self->buffer == NULL) return -1;
	}

	self->capacity = capacity;
	self->length   = 0;

	return 0;
}

static INLINE struct wasm_table *
wasm_table_new(size_t capacity)
{
	struct wasm_table *self = wasm_table_alloc();
	if (self == NULL) return NULL;

	int rc = wasm_table_init(self, capacity);
	if (rc < 0) {
		wasm_table_free(self);
		return NULL;
	}

	return self;
}

static INLINE void
wasm_table_deinit(struct wasm_table *self)
{
	assert(self != NULL);

	if (self->capacity > 0) {
		assert(self->buffer == NULL);
		assert(self->length == 0);
		return;
	}

	assert(self->buffer != NULL);
	free(self->buffer);
	self->buffer   = NULL;
	self->length   = 0;
	self->capacity = 0;
}

static INLINE void
wasm_table_free(struct wasm_table *self)
{
	assert(self != NULL);
	free(self);
}

static INLINE void *
wasm_table_get(struct wasm_table *self, uint32_t idx, uint32_t type_id)
{
	assert(self != NULL);
	assert(idx < self->capacity);

	struct wasm_table_entry f = self->buffer[idx];
	// FIXME: Commented out function type check because of gocr
	// assert(f.type_id == type_id);

	assert(f.func_pointer != NULL);

	return f.func_pointer;
}

static INLINE void
wasm_table_set(struct wasm_table *self, uint32_t idx, uint32_t type_id, char *pointer)
{
	assert(self != NULL);
	assert(idx < self->capacity);
	assert(pointer != NULL);

	/* TODO: atomic for multiple concurrent invocations? Issue #97 */
	if (self->buffer[idx].type_id == type_id && self->buffer[idx].func_pointer == pointer) return;

	self->buffer[idx] = (struct wasm_table_entry){ .type_id = type_id, .func_pointer = pointer };
}
