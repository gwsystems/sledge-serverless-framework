#pragma once

#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "module.h"
#include "route_latency.h"
#include "route.h"
#include "route_config.h"
#include "vec.h"

typedef struct route route_t;
VEC(route_t)

typedef struct vec_route_t http_router_t;

static inline void
http_router_init(http_router_t *router, size_t capacity)
{
	vec_route_t_init(router, capacity);
}

static inline int
http_router_add_route(http_router_t *router, struct route_config *config, struct module *module)
{
	assert(router != NULL);
	assert(config != NULL);
	assert(module != NULL);
	assert(config->route != NULL);
	assert(config->http_resp_content_type != NULL);

	struct route route = { .route                = config->route,
		               .module               = module,
		               .relative_deadline_us = config->relative_deadline_us,
		               .relative_deadline    = (uint64_t)config->relative_deadline_us
		                                    * runtime_processor_speed_MHz,
		               .response_size         = config->http_resp_size,
		               .response_content_type = config->http_resp_content_type };

	route_latency_init(&route.latency);
	http_route_total_init(&route.metrics);

	/* Admissions Control */
	uint64_t expected_execution = (uint64_t)config->expected_execution_us * runtime_processor_speed_MHz;
	admissions_info_initialize(&route.admissions_info, config->admissions_percentile, expected_execution,
	                           route.relative_deadline);

	int rc = vec_route_t_push(router, route);
	if (unlikely(rc == -1)) { return -1; }

	return 0;
}

static inline struct route *
http_router_match_route(http_router_t *router, char *route)
{
	for (int i = 0; i < router->length; i++) {
		if (strncmp(route, router->buffer[i].route, strlen(router->buffer[i].route)) == 0) {
			return &router->buffer[i];
		}
	}

	return NULL;
}

static inline void
http_router_foreach(http_router_t *router, void (*cb)(route_t *, void *, void *), void *arg_one, void *arg_two)
{
	for (int i = 0; i < router->length; i++) { cb(&router->buffer[i], arg_one, arg_two); }
}
