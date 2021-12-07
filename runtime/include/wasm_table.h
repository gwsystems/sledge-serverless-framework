#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

/* memory also provides the table access functions */
#define INDIRECT_TABLE_SIZE (1 << 10)

struct wasm_table_entry {
	uint32_t type_id;
	void *   func_pointer;
};

struct wasm_table {
	uint32_t                length;
	uint32_t                capacity;
	struct wasm_table_entry data[];
};

static inline struct wasm_table *
wasm_table_allocate(size_t capacity)
{
	struct wasm_table *self = (struct wasm_table *)malloc(sizeof(struct wasm_table)
	                                                      + capacity * sizeof(struct wasm_table_entry));

	self->capacity = capacity;
	self->length   = 0;

	return self;
}

static inline void
wasm_table_free(struct wasm_table *self)
{
	assert(self != NULL);
	free(self);
}

static inline void *
wasm_table_get(struct wasm_table *self, uint32_t idx, uint32_t type_id)
{
	assert(self != NULL);
	assert(idx < self->capacity);

	struct wasm_table_entry f = self->data[idx];
	// FIXME: Commented out function type check because of gocr
	// assert(f.type_id == type_id);

	assert(f.func_pointer != NULL);

	return f.func_pointer;
}

static inline void
wasm_table_set(struct wasm_table *self, uint32_t idx, uint32_t type_id, char *pointer)
{
	assert(self != NULL);
	assert(idx < self->capacity);
	assert(pointer != NULL);

	/* TODO: atomic for multiple concurrent invocations? Issue #97 */
	if (self->data[idx].type_id == type_id && self->data[idx].func_pointer == pointer) return;

	self->data[idx] = (struct wasm_table_entry){ .type_id = type_id, .func_pointer = pointer };
}
