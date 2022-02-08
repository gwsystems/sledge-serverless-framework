#pragma once

#include <stdint.h>
#include <stdlib.h>

#define VEC(TYPE)                                                                                                \
                                                                                                                 \
	struct vec_##TYPE {                                                                                      \
		size_t length;                                                                                   \
		size_t capacity;                                                                                 \
		TYPE  *buffer; /* Backing heap allocation. Different lifetime because realloc might move this */ \
	};                                                                                                       \
                                                                                                                 \
	static inline int                vec_##TYPE##_init(struct vec_##TYPE *vec, size_t capacity);             \
	static inline struct vec_##TYPE *vec_##TYPE##_alloc(size_t capacity);                                    \
	static inline void               vec_##TYPE##_deinit(struct vec_##TYPE *vec);                            \
	static inline void               vec_##TYPE##_free(struct vec_##TYPE *vec);                              \
                                                                                                                 \
	/**                                                                                                      \
	 * Initializes a vec, allocating a backing buffer for the provided capcity                               \
	 * @param vec pointer to an uninitialized vec                                                            \
	 * @param capacity                                                                                       \
	 * @returns 0 on success, -1 on failure                                                                  \
	 */                                                                                                      \
	static inline int vec_##TYPE##_init(struct vec_##TYPE *vec, size_t capacity)                             \
	{                                                                                                        \
		if (capacity == 0) {                                                                             \
			vec->buffer = NULL;                                                                      \
		} else {                                                                                         \
			vec->buffer = calloc(capacity, sizeof(TYPE));                                            \
			if (vec->buffer == NULL) return -1;                                                      \
		}                                                                                                \
                                                                                                                 \
		vec->length   = 0;                                                                               \
		vec->capacity = capacity;                                                                        \
                                                                                                                 \
		return 0;                                                                                        \
	}                                                                                                        \
                                                                                                                 \
	/**                                                                                                      \
	 * Allocate and initialize a vec with a backing buffer                                                   \
	 * @param capacity                                                                                       \
	 * @returns a pointer to an initialized vec on the heap, ready for use                                   \
	 */                                                                                                      \
	static inline struct vec_##TYPE *vec_##TYPE##_alloc(size_t capacity)                                     \
	{                                                                                                        \
		struct vec_##TYPE *vec = (struct vec_##TYPE *)malloc(sizeof(struct vec_##TYPE));                 \
		if (vec == NULL) return vec;                                                                     \
                                                                                                                 \
		int rc = vec_##TYPE##_init(vec, capacity);                                                       \
		if (rc < 0) {                                                                                    \
			vec_##TYPE##_free(vec);                                                                  \
			return NULL;                                                                             \
		}                                                                                                \
                                                                                                                 \
		return vec;                                                                                      \
	}                                                                                                        \
                                                                                                                 \
	/**                                                                                                      \
	 * Deinitialize a vec, clearing out members and releasing the backing buffer                             \
	 * @param vec                                                                                            \
	 */                                                                                                      \
	static inline void vec_##TYPE##_deinit(struct vec_##TYPE *vec)                                           \
	{                                                                                                        \
		if (vec->capacity == 0) {                                                                        \
			assert(vec->buffer == NULL);                                                             \
			assert(vec->length == 0);                                                                \
			return;                                                                                  \
		}                                                                                                \
                                                                                                                 \
		assert(vec->buffer != NULL);                                                                     \
		free(vec->buffer);                                                                               \
		vec->buffer   = NULL;                                                                            \
		vec->length   = 0;                                                                               \
		vec->capacity = 0;                                                                               \
	}                                                                                                        \
                                                                                                                 \
	/**                                                                                                      \
	 * Deinitializes and frees a vec allocated to the heap                                                   \
	 * @param vec                                                                                            \
	 */                                                                                                      \
	static inline void vec_##TYPE##_free(struct vec_##TYPE *vec)                                             \
	{                                                                                                        \
		vec_##TYPE##_deinit(vec);                                                                        \
		free(vec);                                                                                       \
	}                                                                                                        \
                                                                                                                 \
	static inline int vec_##TYPE##_insert(struct vec_##TYPE *vec, uint32_t idx, TYPE value)                  \
	{                                                                                                        \
		if (idx >= vec->capacity) return -1;                                                             \
                                                                                                                 \
		vec->buffer[idx] = value;                                                                        \
		return 0;                                                                                        \
	}                                                                                                        \
                                                                                                                 \
	static inline TYPE *vec_##TYPE##_get(struct vec_##TYPE *vec, uint32_t idx)                               \
	{                                                                                                        \
		if (idx >= vec->capacity) return NULL;                                                           \
                                                                                                                 \
		return &vec->buffer[idx];                                                                        \
	}
