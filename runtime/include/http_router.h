#pragma once

#include <stdlib.h>
#include <string.h>

#include "admissions_info.h"
#include "http.h"
#include "module_database.h"
#include "route_config.h"
#include "tcp_server.h"

#define HTTP_ROUTER_ROUTES_CAPACITY 32

/* Assumption: entrypoint is always _start. This should be enhanced later */
struct route {
	char          *route;
	struct module *module;
	/* HTTP State */
	uint32_t               relative_deadline_us;
	uint64_t               relative_deadline; /* cycles */
	size_t                 response_size;
	char                  *response_content_type;
	struct admissions_info admissions_info;
};

struct http_router {
	struct route routes[HTTP_ROUTER_ROUTES_CAPACITY];
	size_t       routes_length;
};

static inline void
http_router_init(struct http_router *router)
{
	router->routes_length = 0;
}

static inline int
http_router_add_route(struct http_router *router, struct route_config *config, struct module *module)
{
	assert(router != NULL);
	assert(config != NULL);
	assert(module != NULL);
	assert(config->route != NULL);
	assert(config->http_resp_content_type != NULL);
	assert(router->routes_length <= HTTP_ROUTER_ROUTES_CAPACITY);

	if (unlikely(router->routes_length == HTTP_ROUTER_ROUTES_CAPACITY)) { return -1; }

	router->routes[router->routes_length] = (struct route){
		.route                 = config->route,
		.module                = module,
		.relative_deadline_us  = config->relative_deadline_us,
		.relative_deadline     = (uint64_t)config->relative_deadline_us * runtime_processor_speed_MHz,
		.response_size         = config->http_resp_size,
		.response_content_type = config->http_resp_content_type
	};

	/* Move strings from config */
	config->route                  = NULL;
	config->http_resp_content_type = NULL;

	/* Admissions Control */
	uint64_t expected_execution = (uint64_t)config->expected_execution_us * runtime_processor_speed_MHz;
	admissions_info_initialize(&router->routes[router->routes_length].admissions_info,
	                           config->admissions_percentile, expected_execution,
	                           router->routes[router->routes_length].relative_deadline);

	router->routes_length++;
	return 0;
}

static inline struct route *
http_router_match_route(struct http_router *router, char *route)
{
	for (int i = 0; i < router->routes_length; i++) {
		if (strncmp(route, router->routes[i].route, strlen(router->routes[i].route)) == 0) {
			return &router->routes[i];
		}
	}

	return NULL;
}
