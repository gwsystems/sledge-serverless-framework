#pragma once

#include <stdio.h>
#include <string.h>

#include "json.h"
#include "route_config.h"

static const char *route_config_json_keys[route_config_member_len] = { "route",
								       "request-type",
	                                                               "path",
	                                                               "admissions-percentile",
	                                                               "expected-execution-us",
	                                                               "relative-deadline-us",
	                                                               "http-resp-content-type" };

static inline int
route_config_set_key_once(bool *did_set, enum route_config_member member)
{
	if (did_set[member]) {
		debuglog("Redundant key %s\n", route_config_json_keys[member]);
		return -1;
	}

	did_set[member] = true;
	return 0;
}

static inline int
route_config_parse(struct route_config *config, const char *json_buf, jsmntok_t *tokens, size_t tokens_base,
                   int tokens_size)
{
	int  i                                = tokens_base;
	char key[32]                          = { 0 };
	bool did_set[route_config_member_len] = { false };

	if (!has_valid_type(tokens[i], "Anonymous Route Config Object", JSMN_OBJECT, json_buf)) return -1;
	if (!is_nonempty_object(tokens[i], "Anonymous Route Config Object")) return -1;

	int route_key_count = tokens[i].size;

	for (int route_key_idx = 0; route_key_idx < route_key_count; route_key_idx++) {
		/* Advance to key */
		i++;

		if (!is_valid_key(tokens[i])) return -1;
		if (!has_valid_size(tokens[i], key, 1)) return -1;

		/* Copy Key */
		sprintf(key, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);

		/* Advance to Value */
		i++;

		if (strcmp(key, route_config_json_keys[route_config_member_route]) == 0) {
			if (!is_nonempty_string(tokens[i], key)) return -1;
			if (route_config_set_key_once(did_set, route_config_member_route) == -1) return -1;

			config->route = strndup(json_buf + tokens[i].start, tokens[i].end - tokens[i].start);
		} else if (strcmp(key, route_config_json_keys[route_config_member_request_type]) == 0) {
                        if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE, json_buf)) return -1;
                        if (route_config_set_key_once(did_set, route_config_member_request_type) == -1)
                                return -1;

                        int rc = parse_uint8_t(tokens[i], json_buf,
                                               route_config_json_keys[route_config_member_request_type],
                                               &config->request_type);
                        if (rc < 0) return -1;
		} else if (strcmp(key, route_config_json_keys[route_config_member_path]) == 0) {
			if (!is_nonempty_string(tokens[i], key)) return -1;
			if (route_config_set_key_once(did_set, route_config_member_path) == -1) return -1;

			config->path = strndup(json_buf + tokens[i].start, tokens[i].end - tokens[i].start);
		} else if (strcmp(key, route_config_json_keys[route_config_member_admissions_percentile]) == 0) {
			if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE, json_buf)) return -1;
			if (route_config_set_key_once(did_set, route_config_member_admissions_percentile) == -1)
				return -1;

			int rc = parse_uint8_t(tokens[i], json_buf,
			                       route_config_json_keys[route_config_member_admissions_percentile],
			                       &config->admissions_percentile);
			if (rc < 0) return -1;
		} else if (strcmp(key, route_config_json_keys[route_config_member_expected_execution_us]) == 0) {
			if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE, json_buf)) return -1;
			if (route_config_set_key_once(did_set, route_config_member_expected_execution_us) == -1)
				return -1;

			int rc = parse_uint32_t(tokens[i], json_buf,
			                        route_config_json_keys[route_config_member_expected_execution_us],
			                        &config->expected_execution_us);
			if (rc < 0) return -1;
		} else if (strcmp(key, route_config_json_keys[route_config_member_relative_deadline_us]) == 0) {
			if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE, json_buf)) return -1;
			if (route_config_set_key_once(did_set, route_config_member_relative_deadline_us) == -1)
				return -1;

			int rc = parse_uint32_t(tokens[i], json_buf,
			                        route_config_json_keys[route_config_member_relative_deadline_us],
			                        &config->relative_deadline_us);
			if (rc < 0) return -1;
		} else if (strcmp(key, route_config_json_keys[route_config_member_http_resp_content_type]) == 0) {
			if (!is_nonempty_string(tokens[i], key)) return -1;
			if (route_config_set_key_once(did_set, route_config_member_http_resp_content_type) == -1)
				return -1;

			config->http_resp_content_type = strndup(json_buf + tokens[i].start,
			                                         tokens[i].end - tokens[i].start);
		} else {
			fprintf(stderr, "%s is not a valid key\n", key);
			return -1;
		}
	}

	if (route_config_validate(config, did_set) < 0) return -1;

	return i;
}
