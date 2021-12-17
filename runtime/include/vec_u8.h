#pragma once

#include <stdint.h>
#include <stdlib.h>

struct vec_u8 {
	size_t   length;
	size_t   capacity;
	uint8_t *buffer; /* Backing heap allocation. Different lifetime because realloc might move this */
};

static inline int            vec_u8_init(struct vec_u8 *vec_u8, size_t capacity);
static inline struct vec_u8 *vec_u8_alloc(size_t capacity);
static inline void           vec_u8_deinit(struct vec_u8 *vec_u8);
static inline void           vec_u8_free(struct vec_u8 *vec_u8);

/**
 * Initializes a vec, allocating a backing buffer for the provided capcity
 * @param vec_u8 pointer to an uninitialized vec
 * @param capacity
 * @returns 0 on success, -1 on failure
 */
static inline int
vec_u8_init(struct vec_u8 *vec_u8, size_t capacity)
{
	if (capacity == 0) {
		vec_u8->buffer = NULL;
	} else {
		vec_u8->buffer = calloc(capacity, sizeof(uint8_t));
		if (vec_u8->buffer == NULL) return -1;
	}

	vec_u8->length   = 0;
	vec_u8->capacity = capacity;

	return 0;
}

/**
 * Allocate and initialize a vec with a backing buffer
 * @param capacity
 * @returns a pointer to an initialized vec on the heap, ready for use
 */
static inline struct vec_u8 *
vec_u8_alloc(size_t capacity)
{
	struct vec_u8 *vec_u8 = (struct vec_u8 *)malloc(sizeof(struct vec_u8));
	if (vec_u8 == NULL) return vec_u8;

	int rc = vec_u8_init(vec_u8, capacity);
	if (rc < 0) {
		vec_u8_free(vec_u8);
		return NULL;
	}

	return vec_u8;
}

/**
 * Deinitialize a vec, clearing out members and releasing the backing buffer
 * @param vec_u8
 */
static inline void
vec_u8_deinit(struct vec_u8 *vec_u8)
{
	if (vec_u8->capacity == 0) {
		assert(vec_u8->buffer == NULL);
		assert(vec_u8->length == 0);
		return;
	}

	assert(vec_u8->buffer != NULL);
	free(vec_u8->buffer);
	vec_u8->buffer   = NULL;
	vec_u8->length   = 0;
	vec_u8->capacity = 0;
}

/**
 * Deinitializes and frees a vec allocated to the heap
 * @param vec_u8
 */
static inline void
vec_u8_free(struct vec_u8 *vec_u8)
{
	vec_u8_deinit(vec_u8);
	free(vec_u8);
}
