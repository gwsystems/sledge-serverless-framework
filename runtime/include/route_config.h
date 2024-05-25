#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "admissions_control.h"
#include "runtime.h"
#include "scheduler_options.h"

enum route_config_member
{
	route_config_member_route,
	route_config_member_path,
	route_config_member_path_premodule,
	route_config_member_admissions_percentile,
	route_config_member_relative_deadline_us,
	route_config_member_model_bias,
	route_config_member_model_scale,
	route_config_member_model_num_of_param,
	route_config_member_model_beta1,
	route_config_member_model_beta2,
	route_config_member_http_resp_content_type,
	route_config_member_len
};

struct route_config {
	char    *route;
	char    *path;
	char    *path_premodule;
	uint8_t  admissions_percentile;
	uint32_t relative_deadline_us;
	uint32_t model_bias;
	uint32_t model_scale;
	uint32_t model_num_of_param;
	uint32_t model_beta1;
	uint32_t model_beta2;
	char    *http_resp_content_type;
};

static inline void
route_config_deinit(struct route_config *config)
{
	/* ownership of the route and http_resp_content_type strings was moved during http_router_add_route */
	assert(config->route == NULL);
	assert(config->http_resp_content_type == NULL);

	/* ownership of the path stringswas moved during module_alloc */
	assert(config->path == NULL);
}

static inline void
route_config_print(struct route_config *config)
{
	printf("[Route] Route: %s\n", config->route);
	printf("[Route] Path: %s\n", config->path);
	printf("[Route] Path of Preprocessing Module: %s\n", config->path_premodule);
	printf("[Route] Admissions Percentile: %hhu\n", config->admissions_percentile);
	printf("[Route] Relative Deadline (us): %u\n", config->relative_deadline_us);
	printf("[Route] Model Bias: %u\n", config->model_bias);
	printf("[Route] Model Scale: %u\n", config->model_scale);
	printf("[Route] Model Num of Parameters: %u\n", config->model_num_of_param);
	printf("[Route] Model Betas: [%u, %u]\n", config->model_beta1, config->model_beta2);
	printf("[Route] HTTP Response Content Type: %s\n", config->http_resp_content_type);
}

/**
 * Validates a route config generated by a parser
 * @param config
 * @param did_set boolean array of size route_config_member_len indicating if parser set the associated member
 */
static inline int
route_config_validate(struct route_config *config, bool *did_set)
{
	if (did_set[route_config_member_route] == false) {
		fprintf(stderr, "path field is required\n");
		return -1;
	}

	if (did_set[route_config_member_path] == false) {
		fprintf(stderr, "path field is required\n");
		return -1;
	}

	if (did_set[route_config_member_path_premodule] == false) {
		fprintf(stderr, "path_premodule field is required\n");
		return -1;
	}

	if (did_set[route_config_member_http_resp_content_type] == false) {
		debuglog("http_resp_content_type not set, defaulting to text/plain\n");
		config->http_resp_content_type = "text/plain";
	}

	if (scheduler != SCHEDULER_FIFO) {
		if (did_set[route_config_member_relative_deadline_us] == false) {
			fprintf(stderr, "relative_deadline_us is required\n");
			return -1;
		}

		if (config->relative_deadline_us > (uint32_t)RUNTIME_RELATIVE_DEADLINE_US_MAX) {
			fprintf(stderr, "Relative-deadline-us must be between 0 and %u, was %u\n",
			        (uint32_t)RUNTIME_RELATIVE_DEADLINE_US_MAX, config->relative_deadline_us);
			return -1;
		}

#ifdef ADMISSIONS_CONTROL
		if (did_set[route_config_member_admissions_percentile] == false) {
			fprintf(stderr, "admissions_percentile is required\n");
			return -1;
		}

		if (config->admissions_percentile > 99 || config->admissions_percentile < 50) {
			fprintf(stderr, "admissions-percentile must be > 50 and <= 99 but was %u\n",
			        config->admissions_percentile);
			return -1;
		}
#endif
	}

	return 0;
}
