#pragma once

#include <stdlib.h>
#include <string.h>

#include "erpc_handler.h"
#include "erpc_c_interface.h"
#include "http.h"
#include "module.h"
#include "route_latency.h"
#include "route.h"
#include "route_config.h"
#include "vec.h"
#include "dispatcher_options.h"

extern struct route *route_array;
typedef struct route route_t;
VEC(route_t)

typedef struct vec_route_t http_router_t;

static inline void
http_router_init(http_router_t *router, size_t capacity)
{
	vec_route_t_init(router, capacity);
        /* Use the request type as the array index, so skip index 0 since request index 
           starts from 1
        */
        route_array = (struct route*) calloc(capacity + 1, sizeof(struct route));
}

static inline int
http_router_add_route(http_router_t *router, struct route_config *config, struct module *module)
{
	assert(router != NULL);
	assert(config != NULL);
	assert(module != NULL);
	assert(config->route != NULL);
	assert(config->http_resp_content_type != NULL);

	struct route route = { .route                	 = config->route,
			       .request_type             = config->request_type,
		               .module                   = module,
		               .relative_deadline_us     = config->relative_deadline_us,
		               .relative_deadline        = (uint64_t)config->relative_deadline_us
		                                            * runtime_processor_speed_MHz,
			       .expected_execution_cycle = config->expected_execution_cycle, 
		               .response_content_type    = config->http_resp_content_type,
			       .group_id                 = config->group_id};

	route_latency_init(&route.latency);
	http_route_total_init(&route.metrics);


	/* Register RPC request handler */
	if (dispatcher == DISPATCHER_EDF_INTERRUPT) {
            if (erpc_register_req_func(config->request_type, edf_interrupt_req_handler, 0) != 0) {
                panic("register erpc function for EDF_INTERRUPT dispatcher failed\n");
	    }
	} else if (dispatcher == DISPATCHER_DARC) {
	    if (erpc_register_req_func(config->request_type, darc_req_handler, 0) != 0) {
                panic("register erpc function for DARC dispatcher failed\n");
            } 
	} else if (dispatcher == DISPATCHER_SHINJUKU) {
            if (erpc_register_req_func(config->request_type, shinjuku_req_handler, 0) != 0) {
		panic("register erpc function for Shinjuku dispatcher failed\n");
            }
        }

	/* Admissions Control */
	uint64_t expected_execution = (uint64_t)config->expected_execution_us * runtime_processor_speed_MHz;
	admissions_info_initialize(&route.admissions_info, config->admissions_percentile, expected_execution,
	                           route.relative_deadline);

	int rc = vec_route_t_push(router, route);
        route_array[config->request_type] = route;
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

static inline struct route *
http_router_match_request_type(http_router_t *router, uint8_t request_type)
{
	/*
        for (int i = 0; i < router->length; i++) {
		if (request_type == router->buffer[i].request_type) {
			return &router->buffer[i];
		}
	}

	return NULL; 
        */

        if (route_array[request_type].request_type == request_type) {
          return &route_array[request_type];
        } else {
          return NULL;
        }
}

static inline void
http_router_foreach(http_router_t *router, void (*cb)(route_t *, void *, void *), void *arg_one, void *arg_two)
{
	for (int i = 0; i < router->length; i++) { cb(&router->buffer[i], arg_one, arg_two); }
}
