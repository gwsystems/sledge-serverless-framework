#pragma once

#include <stdint.h>
#include <string.h>

#include "admissions_info.h"
#include "http.h"
#include "listener_thread.h"
#include "module_database.h"
#include "panic.h"
#include "scheduler_options.h"
#include "tenant.h"
#include "tenant_config.h"

int            tenant_database_add(struct tenant *tenant);
struct tenant *tenant_database_find_by_name(char *name);
struct tenant *tenant_database_find_by_socket_descriptor(int socket_descriptor);
struct tenant *tenant_database_find_by_port(uint16_t port);
struct tenant *tenant_database_find_by_ptr(void *ptr);

static inline struct tenant *
tenant_alloc(struct tenant_config *config)
{
	struct tenant *existing_tenant = tenant_database_find_by_name(config->name);
	if (existing_tenant != NULL) panic("Tenant %s is already initialized\n", existing_tenant->name);

	existing_tenant = tenant_database_find_by_port(config->port);
	if (existing_tenant != NULL)
		panic("Tenant %s is already configured with port %u\n", existing_tenant->name, config->port);

	struct tenant *tenant = (struct tenant *)calloc(1, sizeof(struct tenant));

	/* Move name */
	tenant->name = config->name;
	config->name = NULL;

	tcp_server_init(&tenant->tcp_server, config->port);
	http_router_init(&tenant->router, config->routes_len);
	module_database_init(&tenant->module_db);

	for (int i = 0; i < config->routes_len; i++) {
		/* Resolve module, Ownership of path moves here */
		struct module *module  = module_database_find_by_path(&tenant->module_db, config->routes[i].path);
		config->routes[i].path = NULL;

		assert(module != NULL);

		/* Ownership of config's route and http_resp_content_type move here */
		int rc = http_router_add_route(&tenant->router, &config->routes[i], module);
		if (unlikely(rc != 0)) {
			panic("Tenant %s defined %lu routes, but router failed to grow beyond %lu\n", tenant->name,
			      config->routes_len, tenant->router.capacity);
		}

		config->routes[i].route                  = NULL;
		config->routes[i].http_resp_content_type = NULL;
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
