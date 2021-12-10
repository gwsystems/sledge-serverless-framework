#pragma once

#include <stdint.h>
#include <stdlib.h>

struct vec_u8 {
	size_t   length;
	size_t   capacity;
	uint8_t *buffer;
};

static inline struct vec_u8 *vec_u8_alloc(void);
static inline int            vec_u8_init(struct vec_u8 *self, size_t capacity);
static inline struct vec_u8 *vec_u8_new(size_t capacity);
static inline void           vec_u8_deinit(struct vec_u8 *self);
static inline void           vec_u8_free(struct vec_u8 *self);
static inline void           vec_u8_delete(struct vec_u8 *self);

/**
 * Allocates an uninitialized vec on the heap'
 * @returns a pointer to an uninitialized vec on the heap
 */
static inline struct vec_u8 *
vec_u8_alloc(void)
{
	return (struct vec_u8 *)calloc(1, sizeof(struct vec_u8));
}

/**
 * Initializes a vec, allocating a backing buffer for the provided capcity
 * @param self pointer to an uninitialized vec
 * @param capacity
 * @returns 0 on success, -1 on failure
 */
static inline int
vec_u8_init(struct vec_u8 *self, size_t capacity)
{
	if (capacity == 0) {
		self->buffer = NULL;
	} else {
		self->buffer = calloc(capacity, sizeof(uint8_t));
		if (self->buffer == NULL) return -1;
	}

	self->length   = 0;
	self->capacity = capacity;

	return 0;
}

/**
 * Allocate and initialize a vec with a backing buffer
 * @param capacity
 * @returns a pointer to an initialized vec on the heap, ready for use
 */
static inline struct vec_u8 *
vec_u8_new(size_t capacity)
{
	struct vec_u8 *self = vec_u8_alloc();
	if (self == NULL) return self;

	int rc = vec_u8_init(self, capacity);
	if (rc < 0) {
		vec_u8_free(self);
		return NULL;
	}

	return self;
}

/**
 * Deinitialize a vec, clearing out members and releasing the backing buffer
 * @param self
 */
static inline void
vec_u8_deinit(struct vec_u8 *self)
{
	if (self->capacity == 0) {
		assert(self->buffer == NULL);
		assert(self->length == 0);
	}

	free(self->buffer);
	self->buffer   = NULL;
	self->length   = 0;
	self->capacity = 0;
}

/**
 * Frees a vec struct allocated on the heap
 * Assumes that the vec has already been deinitialized
 */
static inline void
vec_u8_free(struct vec_u8 *self)
{
	assert(self->buffer == NULL);
	assert(self->length == 0);
	assert(self->capacity == 0);
	free(self);
}

/**
 * Deinitializes and frees a vec allocated to the heap
 * @param self
 */
static inline void
vec_u8_delete(struct vec_u8 *self)
{
	vec_u8_deinit(self);
	vec_u8_free(self);
}
