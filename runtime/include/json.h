#pragma once

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <jsmn.h>

#include "runtime.h"
#include "http.h"
#include "module.h"
#include "module_config.h"

#define JSON_TOKENS_CAPACITY 16384

enum
{
	module_name,
	module_path,
	module_port,
	module_expected_execution_us,
	module_replenishment_period_us,
	module_max_budget_us,
	module_admissions_percentile,
	module_relative_deadline_us,
	module_http_req_size,
	module_http_resp_size,
	module_http_resp_content_type,
	module_keys_len
};

static const char *module_keys[module_keys_len] = { "name",
	                                            "path",
	                                            "port",
	                                            "expected-execution-us",
	                                            "replenishment-period-us",
	                                            "max-budget-us",
	                                            "admissions-percentile",
	                                            "relative-deadline-us",
	                                            "http-req-size",
	                                            "http-resp-size",
	                                            "http-resp-content-type" };

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

	jsmntok_t tokens[JSON_TOKENS_CAPACITY];
	int       module_config_vec_len = 0;


	/* Initialize the Jasmine Parser and an array to hold the tokens */
	jsmn_parser module_parser;
	jsmn_init(&module_parser);

	/* Use Jasmine to parse the JSON */
	int total_tokens = jsmn_parse(&module_parser, json_buf, json_buf_size, tokens, JSON_TOKENS_CAPACITY);
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
		char key[32] = { 0 };

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

			if (strcmp(key, module_keys[module_name]) == 0) {
				if (!is_nonempty_string(tokens[i], key)) goto json_parse_err;

				(*module_config_vec)[module_idx].name = strndup(json_buf + tokens[i].start,
				                                                tokens[i].end - tokens[i].start);
			} else if (strcmp(key, module_keys[module_path]) == 0) {
				if (!is_nonempty_string(tokens[i], key)) goto json_parse_err;

				(*module_config_vec)[module_idx].path = strndup(json_buf + tokens[i].start,
				                                                tokens[i].end - tokens[i].start);
			} else if (strcmp(key, module_keys[module_port]) == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				int rc = parse_uint16_t(tokens[i], json_buf, module_keys[module_port],
				                        &(*module_config_vec)[module_idx].port);
				if (rc < 0) goto json_parse_err;
			} else if (strcmp(key, module_keys[module_expected_execution_us]) == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				int rc = parse_uint32_t(tokens[i], json_buf, module_keys[module_expected_execution_us],
				                        &(*module_config_vec)[module_idx].expected_execution_us);
				if (rc < 0) goto json_parse_err;
			} else if (strcmp(key, module_keys[module_replenishment_period_us]) == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				int rc = parse_uint32_t(tokens[i], json_buf,
				                        module_keys[module_replenishment_period_us],
				                        &(*module_config_vec)[module_idx].replenishment_period_us);
				if (rc < 0) goto json_parse_err;
			} else if (strcmp(key, module_keys[module_max_budget_us]) == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				int rc = parse_uint32_t(tokens[i], json_buf, module_keys[module_max_budget_us],
				                        &(*module_config_vec)[module_idx].max_budget_us);
				if (rc < 0) goto json_parse_err;
			} else if (strcmp(key, module_keys[module_admissions_percentile]) == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				int rc = parse_uint8_t(tokens[i], json_buf, module_keys[module_admissions_percentile],
				                       &(*module_config_vec)[module_idx].admissions_percentile);
				if (rc < 0) goto json_parse_err;
			} else if (strcmp(key, module_keys[module_relative_deadline_us]) == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				int rc = parse_uint32_t(tokens[i], json_buf, module_keys[module_relative_deadline_us],
				                        &(*module_config_vec)[module_idx].relative_deadline_us);
				if (rc < 0) goto json_parse_err;
			} else if (strcmp(key, module_keys[module_http_req_size]) == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				int rc = parse_uint32_t(tokens[i], json_buf, module_keys[module_http_req_size],
				                        &(*module_config_vec)[module_idx].http_req_size);
				if (rc < 0) goto json_parse_err;
			} else if (strcmp(key, module_keys[module_http_resp_size]) == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE)) goto json_parse_err;

				int rc = parse_uint32_t(tokens[i], json_buf, module_keys[module_http_resp_size],
				                        &(*module_config_vec)[module_idx].http_resp_size);
				if (rc < 0) goto json_parse_err;
			} else if (strcmp(key, module_keys[module_http_resp_content_type]) == 0) {
				if (!has_valid_type(tokens[i], key, JSMN_STRING)) goto json_parse_err;

				(*module_config_vec)[module_idx].http_resp_content_type =
				  strndup(json_buf + tokens[i].start, tokens[i].end - tokens[i].start);
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
