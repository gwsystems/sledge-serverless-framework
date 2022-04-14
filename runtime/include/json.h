#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <jsmn.h>

#include "runtime.h"
#include "http.h"
#include "module.h"

static const int JSON_MAX_ELEMENT_COUNT = 16;
static const int JSON_MAX_ELEMENT_SIZE  = 1024;

struct module_config {
	char    *name;
	char    *path;
	int      port;
	uint32_t expected_execution_us;
	int      admissions_percentile;
	uint32_t relative_deadline_us;
	int32_t  http_req_size;
	int32_t  http_resp_size;
	char    *http_resp_content_type;
};

static void
print_module_config(struct module_config *config)
{
	printf("Name: %s\n", config->name);
	printf("Path: %s\n", config->path);
	printf("Port: %d\n", config->port);
	printf("expected_execution_us: %u\n", config->expected_execution_us);
	printf("admissions_percentile: %d\n", config->admissions_percentile);
	printf("relative_deadline_us: %u\n", config->relative_deadline_us);
	printf("http_req_size: %u\n", config->http_req_size);
	printf("http_resp_size: %u\n", config->http_resp_size);
	printf("http_resp_content_type: %s\n", config->http_resp_content_type);
}

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
	if (tok.size != 1) {
		fprintf(stderr, "%s does not have a value\n", key);
		return false;
	}

	return true;
}

static inline bool
has_valid_type(jsmntok_t tok, char *key, jsmntype_t expected_type)
{
	if (tok.type != expected_type) {
		fprintf(stderr, "The value of the key %s should be a %s, was a %s\n", key, jsmn_type(expected_type),
		        jsmn_type(tok.type));
		return false;
	}

	return true;
}

static inline bool
is_nonempty_string(jsmntok_t tok, char *key)
{
	if (!has_valid_type(tok, key, JSMN_STRING)) return false;

	if (tok.end - tok.start < 1) {
		fprintf(stderr, "The value of the key %s was an empty string\n", key);
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

/**
 * Parses a JSON file into an array of module configs
 * @param file_name The path of the JSON file
 * @return module_config_vec_len on success. -1 on Error
 */
static inline int
parse_json(const char *json_buf, ssize_t json_buf_size, struct module_config **module_config_vec)
{
	assert(json_buf != NULL);
	assert(json_buf_size > 0);
	assert(module_config_vec != NULL);

	jsmntok_t tokens[JSON_MAX_ELEMENT_SIZE * JSON_MAX_ELEMENT_COUNT];
	int       module_config_vec_len = 0;


	/* Initialize the Jasmine Parser and an array to hold the tokens */
	jsmn_parser module_parser;
	jsmn_init(&module_parser);

	/* Use Jasmine to parse the JSON */
	int total_tokens = jsmn_parse(&module_parser, json_buf, json_buf_size, tokens,
	                              sizeof(tokens) / sizeof(tokens[0]));
	if (total_tokens < 0) {
		if (total_tokens == JSMN_ERROR_INVAL) {
			fprintf(stderr, "Error parsing %s: bad token, JSON string is corrupted\n", json_buf);
		} else if (total_tokens == JSMN_ERROR_PART) {
			fprintf(stderr, "Error parsing %s: JSON string is too short, expecting more JSON data\n",
			        json_buf);
		} else if (total_tokens == JSMN_ERROR_NOMEM) {
			/*
			 * According to the README at https://github.com/zserge/jsmn, this is a potentially recoverable
			 * error. More tokens can be allocated and jsmn_parse can be re-invoked.
			 */
			fprintf(stderr, "Error parsing %s: Not enough tokens, JSON string is too large\n", json_buf);
		}
		goto err;
	}

	if (tokens[0].type != JSMN_ARRAY) {
		fprintf(stderr, "Outermost Config should be a JSON array, was a JSON %s\n", jsmn_type(tokens[0].type));
		goto err;
	}

	module_config_vec_len = tokens[0].size;
	if (module_config_vec_len == 0) {
		fprintf(stderr, "Config is an empty JSON array\n");
		goto err;
	}

	*module_config_vec = (struct module_config *)calloc(module_config_vec_len, sizeof(struct module_config));
	int module_idx     = -1;
	int module_fields_remaining = 0;

	for (int i = 1; i < total_tokens; i++) {
		char key[32]  = { 0 };
		char val[256] = { 0 };

		/* Assumption: Objects are never used within a module_config. This likely will not be true in the
		 * future due to routes or multiple entrypoints */
		if (tokens[i].type == JSMN_OBJECT) {
			assert(module_fields_remaining == 0);
			module_fields_remaining = tokens[i].size;
			module_idx++;
		} else {
			/* Inside Object */

			/* Validate that key is non-emptry string */
			if (!is_valid_key(tokens[i])) goto json_parse_err;

			sprintf(key, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);

			/* Validate that key has a value */
			if (!has_valid_size(tokens[i], key, 1)) goto json_parse_err;

			/* Advance to Value */
			i++;

			if (strcmp(key, "name") == 0) {
				if (!is_nonempty_string(tokens[i], key)) goto json_parse_err;

				sprintf(val, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);
				(*module_config_vec)[module_idx].name = strndup(val, tokens[i].end - tokens[i].start);
			} else if (strcmp(key, "path") == 0) {
				if (!is_nonempty_string(tokens[i], key)) goto json_parse_err;

				sprintf(val, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);
				(*module_config_vec)[module_idx].path = strndup(val, tokens[i].end - tokens[i].start);
			} else if (strcmp(key, "port") == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				sprintf(val, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);
				int buffer = atoi(val);
				if (buffer < 0 || buffer > 65535)
					panic("Expected port between 0 and 65535, saw %d\n", buffer);
				(*module_config_vec)[module_idx].port = buffer;
			} else if (strcmp(key, "expected-execution-us") == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				sprintf(val, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);
				int64_t buffer = strtoll(val, NULL, 10);
				if (buffer < 0 || buffer > (int64_t)RUNTIME_EXPECTED_EXECUTION_US_MAX)
					panic("Relative-deadline-us must be between 0 and %ld, was %ld\n",
					      (int64_t)RUNTIME_EXPECTED_EXECUTION_US_MAX, buffer);
				(*module_config_vec)[module_idx].expected_execution_us = (uint32_t)buffer;
			} else if (strcmp(key, "admissions-percentile") == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				sprintf(val, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);
				int32_t buffer = strtol(val, NULL, 10);
				if (buffer > 99 || buffer < 50)
					panic("admissions-percentile must be > 50 and <= 99 but was %d\n", buffer);
				(*module_config_vec)[module_idx].admissions_percentile = buffer;
			} else if (strcmp(key, "relative-deadline-us") == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				sprintf(val, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);
				int64_t buffer = strtoll(val, NULL, 10);
				if (buffer < 0 || buffer > (int64_t)RUNTIME_RELATIVE_DEADLINE_US_MAX)
					panic("Relative-deadline-us must be between 0 and %ld, was %ld\n",
					      (int64_t)RUNTIME_RELATIVE_DEADLINE_US_MAX, buffer);
				(*module_config_vec)[module_idx].relative_deadline_us = (uint32_t)buffer;
			} else if (strcmp(key, "http-req-size") == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				sprintf(val, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);
				int64_t buffer = strtoll(val, NULL, 10);
				if (buffer < 0 || buffer > RUNTIME_HTTP_REQUEST_SIZE_MAX)
					panic("http-req-size must be between 0 and %ld, was %ld\n",
					      (int64_t)RUNTIME_HTTP_REQUEST_SIZE_MAX, buffer);
				(*module_config_vec)[module_idx].http_req_size = (int32_t)buffer;
			} else if (strcmp(key, "http-resp-size") == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				sprintf(val, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);
				int64_t buffer = strtoll(val, NULL, 10);
				if (buffer < 0 || buffer > RUNTIME_HTTP_REQUEST_SIZE_MAX)
					panic("http-req-size must be between 0 and %ld, was %ld\n",
					      (int64_t)RUNTIME_HTTP_RESPONSE_SIZE_MAX, buffer);
				(*module_config_vec)[module_idx].http_resp_size = (int32_t)buffer;
			} else if (strcmp(key, "http-resp-content-type") == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_STRING)) goto json_parse_err;

				sprintf(val, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);
				(*module_config_vec)[module_idx].http_resp_content_type = strndup(val,
				                                                                  tokens[i].end
				                                                                    - tokens[i].start);
			} else {
				fprintf(stderr, "%s is not a valid key\n", key);
				goto json_parse_err;
			}
			module_fields_remaining--;
		}
	}

#ifdef LOG_MODULE_LOADING
	debuglog("Loaded %d module%s!\n", module_count, module_count > 1 ? "s" : "");
#endif

done:
	return module_config_vec_len;
json_parse_err:
	free(*module_config_vec);
err:
	fprintf(stderr, "JSON:\n%s\n", json_buf);
	module_config_vec_len = -1;
	goto done;
}
