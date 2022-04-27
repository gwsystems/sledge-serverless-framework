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
