#pragma once


#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <jsmn.h>

static inline char *
jsmn_type(jsmntype_t type)
{
	switch (type) {
	case JSMN_UNDEFINED:
		return "Undefined";
	case JSMN_OBJECT:
		return "Object";
	case JSMN_ARRAY:
		return "Array";
	case JSMN_STRING:
		return "String";
	case JSMN_PRIMITIVE:
		return "Primitive";
	default:
		return "Invalid";
	}
}

static inline bool
has_valid_size(jsmntok_t tok, char *key, int expected_size)
{
	if (tok.size != expected_size) {
		fprintf(stderr, "%s has size %d, expected %d\n", key, tok.size, expected_size);
		return false;
	}

	return true;
}

static inline bool
has_valid_type(jsmntok_t tok, char *key, jsmntype_t expected_type, const char *json_buf)
{
	if (tok.type != expected_type) {
		fprintf(stderr, "The value of the key %s should be a %s, was a %s\n", key, jsmn_type(expected_type),
		        jsmn_type(tok.type));
		if (json_buf != NULL)
			fprintf(stderr, "String of value %.*s\n", tok.end - tok.start, &json_buf[tok.start]);

		return false;
	}

	return true;
}

static inline bool
is_nonempty_string(jsmntok_t tok, char *key)
{
	if (!has_valid_type(tok, key, JSMN_STRING, NULL)) return false;

	if (tok.end - tok.start < 1) {
		fprintf(stderr, "The value of the key %s was an empty string\n", key);
		return false;
	}

	return true;
}

static inline bool
is_nonempty_object(jsmntok_t tok, char *key)
{
	if (!has_valid_type(tok, key, JSMN_OBJECT, NULL)) return false;

	if (tok.size == 0) {
		fprintf(stderr, "The value of the key %s was an empty object\n", key);
		return false;
	}

	return true;
}

static inline bool
is_nonempty_array(jsmntok_t tok, char *key)
{
	if (!has_valid_type(tok, key, JSMN_ARRAY, NULL)) return false;

	if (tok.size == 0) {
		fprintf(stderr, "The value of the key %s was an empty array\n", key);
		return false;
	}

	return true;
}

static inline bool
is_valid_key(jsmntok_t tok)
{
	if (tok.type != JSMN_STRING) {
		fprintf(stderr, "Expected token to be a key with a type of string, was a %s\n", jsmn_type(tok.type));
		return false;
	}

	if (tok.end - tok.start < 1) {
		fprintf(stderr, "Key was an empty string\n");
		return false;
	}

	return true;
}

static inline int
parse_uint8_t(jsmntok_t tok, const char *json_buf, const char *key, uint8_t *ret)
{
	char    *end  = NULL;
	intmax_t temp = strtoimax(&json_buf[tok.start], &end, 10);

	if (end != &json_buf[tok.end] || temp < 0 || temp > UINT8_MAX) {
		fprintf(stderr, "Unable to parse uint8_t for key %s\n", key);
		return -1;
	}

	*ret = (uint8_t)temp;
	return 0;
}

static inline int
parse_uint16_t(jsmntok_t tok, const char *json_buf, const char *key, uint16_t *ret)
{
	char    *end  = NULL;
	intmax_t temp = strtoimax(&json_buf[tok.start], &end, 10);

	if (end != &json_buf[tok.end] || temp < 0 || temp > UINT16_MAX) {
		fprintf(stderr, "Unable to parse uint16_t for key %s\n", key);
		return -1;
	}

	*ret = (uint16_t)temp;
	return 0;
}

static inline int
parse_uint32_t(jsmntok_t tok, const char *json_buf, const char *key, uint32_t *ret)
{
	char     *end  = NULL;
	uintmax_t temp = strtoimax(&json_buf[tok.start], &end, 10);

	if (end != &json_buf[tok.end] || temp < 0 || temp > UINT32_MAX) {
		fprintf(stderr, "Unable to parse uint32_t for key %s\n", key);
		return -1;
	}

	*ret = (uint32_t)temp;
	return 0;
}

static inline int
parse_uint64_t(jsmntok_t tok, const char *json_buf, const char *key, uint64_t *ret)
{
	char     *end  = NULL;
	uintmax_t temp = strtoimax(&json_buf[tok.start], &end, 10);

	if (end != &json_buf[tok.end] || temp < 0 || temp > UINT64_MAX) {
		fprintf(stderr, "Unable to parse uint32_t for key %s\n", key);
		return -1;
	}

	*ret = (uint64_t)temp;
	return 0;
}
