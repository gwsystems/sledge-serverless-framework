#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "lock.h"
#include "xmalloc.h"

/* Simple K-V store based on The Practice of Programming by Kernighan and Pike */

/* Bucket count is sized to be a prime that is approximately 20% larger than the desired capacity (6k keys) */
#define MAP_BUCKET_COUNT 7907
#define MAP_HASH         jenkins_hash

struct map_node {
	struct map_node *next;
	uint8_t         *key;
	uint8_t         *value;
	uint32_t         key_len;
	uint32_t         value_len;
	uint32_t         hash;
};

struct map_bucket {
	lock_t           lock;
	struct map_node *head;
};

struct map {
	struct map_bucket buckets[MAP_BUCKET_COUNT];
};

static inline void
map_init(struct map *restrict map)
{
	for (int i = 0; i < MAP_BUCKET_COUNT; i++) {
		map->buckets[i].head = NULL;
		lock_init(&map->buckets[i].lock);
	}
};

/* See https://en.wikipedia.org/wiki/Jenkins_hash_function */
static inline uint32_t
jenkins_hash(uint8_t *key, uint32_t key_len)
{
	uint32_t i    = 0;
	uint32_t hash = 0;
	while (i != key_len) {
		hash += key[i++];
		hash += hash << 10;
		hash ^= hash >> 6;
	}
	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;
	return hash;
}

static inline uint8_t *
map_get(struct map *map, uint8_t *key, uint32_t key_len, uint32_t *ret_value_len)
{
	uint8_t *value = NULL;

	uint32_t hash = MAP_HASH(key, key_len);

	struct map_bucket *bucket = &map->buckets[hash % MAP_BUCKET_COUNT];

	lock_node_t node = {};
	lock_lock(&bucket->lock, &node);
	for (struct map_node *node = bucket->head; node != NULL; node = node->next) {
		if (node->hash == hash) {
			value          = node->value;
			*ret_value_len = node->value_len;
			goto DONE;
		}
	}

	if (value == NULL) *ret_value_len = 0;

DONE:
	lock_unlock(&bucket->lock, &node);
	return value;
}

static inline bool
map_set(struct map *map, uint8_t *key, uint32_t key_len, uint8_t *value, uint32_t value_len)
{
	bool did_set = false;

	uint32_t           hash   = MAP_HASH(key, key_len);
	struct map_bucket *bucket = &map->buckets[hash % MAP_BUCKET_COUNT];
	lock_node_t        node;
	lock_lock(&bucket->lock, &node);
	for (struct map_node *node = bucket->head; node != NULL; node = node->next) {
		if (node->hash == hash) goto DONE;
	}

	struct map_node *new_node = (struct map_node *)xmalloc(sizeof(struct map_node));
	*(new_node)               = (struct map_node){ .hash      = hash,
		                                       .key       = xmalloc(key_len),
		                                       .key_len   = key_len,
		                                       .value     = xmalloc(value_len),
		                                       .value_len = value_len,
		                                       .next      = bucket->head };

	// Copy Key and Value
	memcpy(new_node->key, key, key_len);
	memcpy(new_node->value, value, value_len);

	bucket->head = new_node;
	did_set      = true;

DONE:
	lock_unlock(&bucket->lock, &node);
	return did_set;
}

/**
 * @returns boolean if node was deleted or not
 */
static inline bool
map_delete(struct map *map, uint8_t *key, uint32_t key_len)
{
	bool did_delete = false;

	uint32_t           hash   = MAP_HASH(key, key_len);
	struct map_bucket *bucket = &map->buckets[hash % MAP_BUCKET_COUNT];
	lock_node_t        node;
	lock_lock(&bucket->lock, &node);

	struct map_node *prev = bucket->head;
	if (prev != NULL && prev->hash == hash) {
		bucket->head = prev->next;
		free(prev->key);
		free(prev->value);
		free(prev);
		did_delete = true;
		goto DONE;
	}

	for (struct map_node *node = prev->next; node != NULL; prev = node, node = node->next) {
		prev->next = node->next;
		free(node->key);
		free(node->value);
		free(node);
		did_delete = true;
		goto DONE;
	}

DONE:
	lock_unlock(&bucket->lock, &node);
	return did_delete;
}

static inline void
map_upsert(struct map *map, uint8_t *key, uint32_t key_len, uint8_t *value, uint32_t value_len)
{
	uint32_t           hash   = MAP_HASH(key, key_len);
	struct map_bucket *bucket = &map->buckets[hash % MAP_BUCKET_COUNT];
	lock_node_t        node;
	lock_lock(&bucket->lock, &node);

	for (struct map_node *node = bucket->head; node != NULL; node = node->next) {
		if (node->hash == hash) {
			node->value_len = value_len;
			node->value     = realloc(node->value, value_len);
			assert(node->value);
			memcpy(node->value, value, value_len);
			goto DONE;
		}
	}

	struct map_node *new_node = (struct map_node *)xmalloc(sizeof(struct map_node));

	*(new_node) = (struct map_node){ .hash      = hash,
		                         .key       = xmalloc(key_len),
		                         .key_len   = key_len,
		                         .value     = xmalloc(value_len),
		                         .value_len = value_len,
		                         .next      = bucket->head };

	assert(new_node->key);
	assert(new_node->value);

	// Copy Key and Value
	memcpy(new_node->key, key, key_len);
	memcpy(new_node->value, value, value_len);

	bucket->head = new_node;

DONE:
	lock_unlock(&bucket->lock, &node);
}
