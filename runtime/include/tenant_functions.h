#pragma once

#include <stdint.h>
#include <string.h>

#include "admissions_info.h"
#include "http.h"
#include "listener_thread.h"
#include "panic.h"
#include "scheduler_options.h"
#include "tenant.h"
#include "tenant_config.h"

int            tenant_database_add(struct tenant *tenant);
struct tenant *tenant_database_find_by_name(char *name);
struct tenant *tenant_database_find_by_socket_descriptor(int socket_descriptor);
struct tenant *tenant_database_find_by_port(uint16_t port);

static inline struct tenant *
tenant_alloc(struct tenant_config *config)
{
	/* Validate config */
	if (strlen(config->name) == 0) panic("name field is required\n");
	if (config->port == 0) panic("port field is required\n");
	if (config->routes_len == 0) panic("one or more routesa are required\n");

	struct tenant *existing_tenant = tenant_database_find_by_name(config->name);
	if (existing_tenant != NULL) panic("Tenant %s is already initialized\n", existing_tenant->name);

	existing_tenant = tenant_database_find_by_port(config->port);
	if (existing_tenant != NULL)
		panic("Tenant %s is already configured with port %u\n", existing_tenant->name, config->port);

	for (int i = 0; i < config->routes_len; i++) {
		struct route_config *route_config = &config->routes[i];
		if (route_config->path == 0) panic("path field is required\n");
		if (route_config->route == 0) panic("route field is required\n");


		if (route_config->http_req_size > RUNTIME_HTTP_REQUEST_SIZE_MAX)
			panic("request_size must be between 0 and %u, was %u\n",
			      (uint32_t)RUNTIME_HTTP_REQUEST_SIZE_MAX, route_config->http_req_size);

		if (route_config->http_resp_size > RUNTIME_HTTP_RESPONSE_SIZE_MAX)
			panic("response-size must be between 0 and %u, was %u\n",
			      (uint32_t)RUNTIME_HTTP_RESPONSE_SIZE_MAX, route_config->http_resp_size);

		if (route_config->relative_deadline_us > (uint32_t)RUNTIME_RELATIVE_DEADLINE_US_MAX)
			panic("Relative-deadline-us must be between 0 and %u, was %u\n",
			      (uint32_t)RUNTIME_RELATIVE_DEADLINE_US_MAX, route_config->relative_deadline_us);

#ifdef ADMISSIONS_CONTROL
		/* expected-execution-us and relative-deadline-us are required in case of admissions control */
		if (route_config->expected_execution_us == 0) panic("expected-execution-us is required\n");
		if (route_config->relative_deadline_us == 0) panic("relative_deadline_us is required\n");

		if (route_config->admissions_percentile > 99 || route_config->admissions_percentile < 50)
			panic("admissions-percentile must be > 50 and <= 99 but was %u\n",
			      route_config->admissions_percentile);

		/* If the ratio is too big, admissions control is too coarse */
		uint32_t ratio = route_config->relative_deadline_us / route_config->expected_execution_us;
		if (ratio > ADMISSIONS_CONTROL_GRANULARITY)
			panic("Ratio of Deadline to Execution time cannot exceed admissions control "
			      "granularity of "
			      "%d\n",
			      ADMISSIONS_CONTROL_GRANULARITY);
#else
		/* relative-deadline-us is required if scheduler is EDF */
		if (scheduler == SCHEDULER_EDF && route_config->relative_deadline_us == 0)
			panic("relative_deadline_us is required\n");
#endif
	}


	struct tenant *tenant = (struct tenant *)calloc(1, sizeof(struct tenant));

	/* Move name */
	tenant->name = config->name;
	config->name = NULL;

	tcp_server_init(&tenant->tcp_server, config->port);
	http_router_init(&tenant->router);
	module_database_init(&tenant->module_db);

	for (int i = 0; i < config->routes_len; i++) {
		/* Resolve module */
		struct module *module = module_database_find_by_path(&tenant->module_db, config->routes[i].path);
		assert(module != NULL);
		http_router_add_route(&tenant->router, &config->routes[i], module);
	}

	return tenant;
}

/**
 * Start the tenant as a server listening at tenant->port
 * @param tenant
 * @returns 0 on success, -1 on error
 */
int tenant_listen(struct tenant *tenant);
int listener_thread_register_tenant(struct tenant *tenant);
