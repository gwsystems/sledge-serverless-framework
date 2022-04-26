#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct route_config {
	char    *path;
	char    *route;
	uint8_t  admissions_percentile;
	uint32_t expected_execution_us;
	uint32_t relative_deadline_us;
	uint32_t http_resp_size;
	char    *http_resp_content_type;
};

static inline void
route_config_deinit(struct route_config *config)
{
	free(config->path);
	free(config->route);
	free(config->http_resp_content_type);
}

static inline void
route_config_print(struct route_config *config)
{
	printf("[Route] Route: %s\n", config->route);
	printf("[Route] Path: %s\n", config->path);
	printf("[Route] Admissions Percentile: %hhu\n", config->admissions_percentile);
	printf("[Route] Expected Execution (us): %u\n", config->expected_execution_us);
	printf("[Route] Relative Deadline (us): %u\n", config->relative_deadline_us);
	printf("[Route] HTTP Response Size: %u\n", config->http_resp_size);
	printf("[Route] HTTP Response Content Type: %s\n", config->http_resp_content_type);
}
