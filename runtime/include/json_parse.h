#pragma once

#include <assert.h>
#include <jsmn.h>
#include <stdio.h>
#include <stdlib.h>

#include "tenant_config_parse.h"

#define JSON_TOKENS_CAPACITY 16384

/**
 * Parses a JSON file into an array of tenant configs
 * @param file_name The path of the JSON file
 * @return tenant_config_vec_len on success. -1 on Error
 */
static inline int
parse_json(const char *json_buf, ssize_t json_buf_size, struct tenant_config **tenant_config_vec)
{
	assert(json_buf != NULL);
	assert(json_buf_size > 0);
	assert(tenant_config_vec != NULL);

	jsmntok_t tokens[JSON_TOKENS_CAPACITY];
	int       tenant_config_vec_len = 0;
	int       i                     = 0;

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

	i = tenant_config_vec_parse(tenant_config_vec, &tenant_config_vec_len, json_buf, tokens, i, total_tokens);

	assert(i == total_tokens - 1);

done:
	return tenant_config_vec_len;
json_parse_err:
	free(*tenant_config_vec);
err:
	fprintf(stderr, "JSON:\n%s\n", json_buf);
	tenant_config_vec_len = -1;
	goto done;
}
