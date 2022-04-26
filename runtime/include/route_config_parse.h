#pragma once

#include <stdio.h>
#include <string.h>

#include "json.h"
#include "route_config.h"

enum
{
	route_config_json_key_http_path,
	route_config_json_key_module_path,
	route_config_json_key_admissions_percentile,
	route_config_json_key_expected_execution_us,
	route_config_json_key_relative_deadline_us,
	route_config_json_key_http_resp_size,
	route_config_json_key_http_resp_content_type,
	route_config_json_key_len
};

static const char *route_config_json_keys[route_config_json_key_len] = { "route",
	                                                                 "path",
	                                                                 "admissions-percentile",
	                                                                 "expected-execution-us",
	                                                                 "relative-deadline-us",
	                                                                 "http-resp-size",
	                                                                 "http-resp-content-type" };

static inline int
route_config_parse(struct route_config *config, const char *json_buf, jsmntok_t *tokens, size_t tokens_base,
                   int tokens_size)
{
	int  i       = tokens_base;
	char key[32] = { 0 };

	if (!has_valid_type(tokens[i], "anonymous object in array", JSMN_OBJECT, json_buf)) return -1;

	int route_keys_len = tokens[i].size;

	if (tokens[i].size == 0) {
		fprintf(stderr, "empty route object\n");
		return -1;
	}

	for (int route_key_idx = 0; route_key_idx < route_keys_len; route_key_idx++) {
		i++;
		if (!is_valid_key(tokens[i])) return -1;
		sprintf(key, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);

		/* Advance to Value */
		i++;

		if (strcmp(key, route_config_json_keys[route_config_json_key_http_path]) == 0) {
			if (!is_nonempty_string(tokens[i], key)) return -1;

			config->route = strndup(json_buf + tokens[i].start, tokens[i].end - tokens[i].start);
		} else if (strcmp(key, route_config_json_keys[route_config_json_key_module_path]) == 0) {
			if (!is_nonempty_string(tokens[i], key)) return -1;

			config->path = strndup(json_buf + tokens[i].start, tokens[i].end - tokens[i].start);
		} else if (strcmp(key, route_config_json_keys[route_config_json_key_admissions_percentile]) == 0) {
			if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE, json_buf)) return -1;

			int rc = parse_uint8_t(tokens[i], json_buf,
			                       route_config_json_keys[route_config_json_key_admissions_percentile],
			                       &config->admissions_percentile);
			if (rc < 0) return -1;
		} else if (strcmp(key, route_config_json_keys[route_config_json_key_expected_execution_us]) == 0) {
			if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE, json_buf)) return -1;

			int rc = parse_uint32_t(tokens[i], json_buf,
			                        route_config_json_keys[route_config_json_key_expected_execution_us],
			                        &config->expected_execution_us);
			if (rc < 0) return -1;
		} else if (strcmp(key, route_config_json_keys[route_config_json_key_relative_deadline_us]) == 0) {
			if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE, json_buf)) return -1;

			int rc = parse_uint32_t(tokens[i], json_buf,
			                        route_config_json_keys[route_config_json_key_relative_deadline_us],
			                        &config->relative_deadline_us);
			if (rc < 0) return -1;
		} else if (strcmp(key, route_config_json_keys[route_config_json_key_http_resp_size]) == 0) {
			if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE, json_buf)) return -1;

			int rc = parse_uint32_t(tokens[i], json_buf,
			                        route_config_json_keys[route_config_json_key_http_resp_size],
			                        &config->http_resp_size);
			if (rc < 0) return -1;
		} else if (strcmp(key, route_config_json_keys[route_config_json_key_http_resp_content_type]) == 0) {
			if (!is_nonempty_string(tokens[i], key)) return -1;

			config->http_resp_content_type = strndup(json_buf + tokens[i].start,
			                                         tokens[i].end - tokens[i].start);
		} else {
			fprintf(stderr, "%s is not a valid key\n", key);
			return -1;
		}
	}

	return i;
}
