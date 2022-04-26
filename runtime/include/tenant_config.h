#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "route_config.h"

struct tenant_config {
	char	        *name;
	uint16_t             port;
	struct route_config *routes;
	size_t               routes_len;
};

static inline void
tenant_config_deinit(struct tenant_config *config)
{
	if (config->name != NULL) free(config->name);
	config->name = NULL;
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
	printf("[Tenant] Routes Size: %zu\n", config->routes_len);
	for (int i = 0; i < config->routes_len; i++) { route_config_print(&config->routes[i]); }
}
