#pragma once

#include <stdlib.h>
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
http_router_add_route(http_router_t *router, struct route_config *config, struct module *module,
                      struct module *module_proprocess)
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
		               .response_content_type = config->http_resp_content_type };

	route_latency_init(&route.latency);
	http_route_total_init(&route.metrics);

#ifdef EXECUTION_REGRESSION
	/* Execution Regression setup */
	route.module_proprocess       = module_proprocess;
	route.regr_model.bias         = config->model_bias / 1000.0;
	route.regr_model.scale        = config->model_scale / 1000.0;
	route.regr_model.num_of_param = config->model_num_of_param;
	route.regr_model.beta1        = config->model_beta1 / 1000.0;
	route.regr_model.beta2        = config->model_beta2 / 1000.0;
#endif

	const uint64_t expected_execution = route.relative_deadline / 2;
#ifdef ADMISSIONS_CONTROL
	/* Addmissions Control setup */
	route.execution_histogram.estimated_execution = expected_execution;
#endif

#ifdef EXECUTION_HISTOGRAM
	/* Execution Histogram setup */
	execution_histogram_initialize(&route.execution_histogram, config->admissions_percentile, expected_execution);
#endif

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
