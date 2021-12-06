#pragma once

#include <stdint.h>
#include <stdlib.h>

struct vec_u8 {
	size_t  length;
	size_t  capacity;
	uint8_t buffer[];
};

static inline struct vec_u8 *
vec_u8_alloc(size_t capacity)
{
	return (struct vec_u8 *)calloc(1, sizeof(struct vec_u8) + capacity * sizeof(uint8_t));
}

static inline void
vec_u8_init(struct vec_u8 *self, size_t capacity)
{
	self->length   = 0;
	self->capacity = capacity;
}

static inline struct vec_u8 *
vec_u8_new(size_t capacity)
{
	struct vec_u8 *self = vec_u8_alloc(capacity);
	vec_u8_init(self, capacity);
	return self;
}

static inline void
vec_u8_free(struct vec_u8 *self)
{
	free(self);
}
