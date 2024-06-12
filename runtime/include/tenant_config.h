#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "panic.h"
#include "route_config.h"
#include "runtime.h"

enum tenant_config_member
{
	tenant_config_member_name,
	tenant_config_member_port,
	tenant_config_member_replenishment_period_us,
	tenant_config_member_max_budget_us,
	tenant_config_member_routes,
	tenant_config_member_len
};

struct tenant_config {
	char                *name;
	uint16_t             port;
	uint32_t             replenishment_period_us;
	uint32_t             max_budget_us;
	struct route_config *routes;
	size_t               routes_len;
};

static inline void
tenant_config_deinit(struct tenant_config *config)
{
	if (config->name != NULL) free(config->name);
	config->name                    = NULL;
	config->replenishment_period_us = 0;
	config->max_budget_us           = 0;
	for (int i = 0; i < config->routes_len; i++) { route_config_deinit(&config->routes[i]); }
	free(config->routes);
	config->routes     = NULL;
	config->routes_len = 0;
}

static inline void
tenant_config_print(struct tenant_config *config)
{
	printf("[Tenant] Name: %s\n", config->name);
	printf("[Tenant] Path: %d\n", config->port);
	if (scheduler == SCHEDULER_MTDS) {
		printf("[Tenant] Replenishment Period (us): %u\n", config->replenishment_period_us);
		printf("[Tenant] Max Budget (us): %u\n", config->max_budget_us);
	}
	printf("[Tenant] Routes Size: %zu\n", config->routes_len);
	for (int i = 0; i < config->routes_len; i++) { route_config_print(&config->routes[i]); }
}

static inline int
tenant_config_validate(struct tenant_config *config, bool *did_set)
{
	if (did_set[tenant_config_member_name] == false) {
		fprintf(stderr, "name field is required\n");
		return -1;
	}

	if (strlen(config->name) == 0) {
		fprintf(stderr, "name field is empty string\n");
		return -1;
	}

	if (did_set[tenant_config_member_port] == false) {
		fprintf(stderr, "port field is required\n");
		return -1;
	}

	if (scheduler == SCHEDULER_MTDS) {
		if (did_set[tenant_config_member_replenishment_period_us] == false) {
			fprintf(stderr, "replenishment-period-us field is missing, so defaulting to 0\n");
			config->replenishment_period_us = 0;
		}

		if (config->replenishment_period_us > (uint32_t)RUNTIME_RELATIVE_DEADLINE_US_MAX) {
			fprintf(stderr, "relative-deadline-us must be between 0 and %u, was %u\n",
			        (uint32_t)RUNTIME_RELATIVE_DEADLINE_US_MAX, config->replenishment_period_us);
			return -1;
		}

		if (did_set[tenant_config_member_max_budget_us] == false) {
			fprintf(stderr, "max-budget-us field is missing, so defaulting to 0\n");
			config->max_budget_us = 0;
		}

		if (config->max_budget_us > (uint32_t)RUNTIME_RELATIVE_DEADLINE_US_MAX) {
			fprintf(stderr, "Max-budget-us must be between 0 and %u, was %u\n",
			        (uint32_t)RUNTIME_RELATIVE_DEADLINE_US_MAX, config->max_budget_us);
			return -1;
		}
	}

	if (config->routes_len == 0) {
		fprintf(stderr, "one or more routes are required\n");
		return -1;
	}

	return 0;
}
