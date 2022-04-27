#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debuglog.h"
#include "json.h"
#include "route_config_parse.h"
#include "tenant_config.h"

static const char *tenant_config_json_keys[tenant_config_member_len] = { "name", "port", "routes" };

static inline int
tenant_config_set_key_once(bool *did_set, enum tenant_config_member member)
{
	if (did_set[member]) {
		debuglog("Redundant key %s\n", tenant_config_json_keys[member]);
		return -1;
	}

	did_set[member] = true;
	return 0;
}

static inline int
tenant_config_parse(struct tenant_config *config, const char *json_buf, jsmntok_t *tokens, size_t tokens_base,
                    int tokens_size)
{
	int  i                                 = tokens_base;
	char key[32]                           = { 0 };
	bool did_set[tenant_config_member_len] = { false };

	if (!has_valid_type(tokens[i], "Anonymous Tenant Config Object", JSMN_OBJECT, json_buf)) return -1;
	if (!is_nonempty_object(tokens[i], "Anonymous Tenant Config Object")) return -1;

	int tenant_key_count = tokens[i].size;

	for (int tenant_key_idx = 0; tenant_key_idx < tenant_key_count; tenant_key_idx++) {
		/* Advance to key */
		i++;

		if (!is_valid_key(tokens[i])) return -1;
		if (!has_valid_size(tokens[i], key, 1)) return -1;

		/* Copy Key */
		sprintf(key, "%.*s", tokens[i].end - tokens[i].start, json_buf + tokens[i].start);

		/* Advance to Value */
		i++;

		if (strcmp(key, tenant_config_json_keys[tenant_config_member_name]) == 0) {
			if (!is_nonempty_string(tokens[i], key)) return -1;
			if (tenant_config_set_key_once(did_set, tenant_config_member_name) == -1) return -1;

			config->name = strndup(json_buf + tokens[i].start, tokens[i].end - tokens[i].start);
		} else if (strcmp(key, tenant_config_json_keys[tenant_config_member_port]) == 0) {
			if (!has_valid_type(tokens[i], key, JSMN_PRIMITIVE, json_buf)) return -1;
			if (tenant_config_set_key_once(did_set, tenant_config_member_port) == -1) return -1;

			int rc = parse_uint16_t(tokens[i], json_buf, tenant_config_json_keys[tenant_config_member_port],
			                        &config->port);
			if (rc < 0) return -1;
		} else if (strcmp(key, tenant_config_json_keys[tenant_config_member_routes]) == 0) {
			if (!has_valid_type(tokens[i], key, JSMN_ARRAY, json_buf)) return -1;
			if (tenant_config_set_key_once(did_set, tenant_config_member_routes) == -1) return -1;

			int routes_len     = tokens[i].size;
			config->routes_len = routes_len;
			config->routes     = (struct route_config *)calloc(routes_len, sizeof(struct route_config));

			for (int route_idx = 0; route_idx < routes_len; route_idx++) {
				/* Advance to object */
				i++;
				i = route_config_parse(&(config->routes)[route_idx], json_buf, tokens, i, tokens_size);
				if (i == -1) return -1;
			}

		} else {
			fprintf(stderr, "%s is not a valid key\n", key);
			return -1;
		}
	}

	if (tenant_config_validate(config, did_set) < 0) return -1;

	return i;
}

/* Tenant Config Vec */

static inline int
tenant_config_vec_parse(struct tenant_config **tenant_config_vec, int *tenant_config_vec_len, const char *json_buf,
                        jsmntok_t *tokens, size_t tokens_base, int tokens_size)
{
	int i = tokens_base;

	if (tokens[i].type != JSMN_ARRAY) {
		fprintf(stderr, "Outermost Config should be a JSON array, was a JSON %s\n", jsmn_type(tokens[0].type));
		return -1;
	}

	*tenant_config_vec_len = tokens[i].size;
	if (tenant_config_vec_len == 0) {
		fprintf(stderr, "Config is an empty JSON array\n");
		return -1;
	}

	*tenant_config_vec = (struct tenant_config *)calloc((size_t)(*tenant_config_vec_len),
	                                                    sizeof(struct tenant_config));
	if (*tenant_config_vec == NULL) {
		perror("Failed to allocate vec");
		return -1;
	}


	for (int tenant_idx = 0; tenant_idx < *tenant_config_vec_len; tenant_idx++) {
		i++;
		i = tenant_config_parse(&((*tenant_config_vec)[tenant_idx]), json_buf, tokens, i, tokens_size);
		if (i == -1) return -1;
	}

	return i;
}
