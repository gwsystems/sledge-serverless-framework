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
#include "priority_queue.h"
#include "sandbox_functions.h"

int            tenant_database_add(struct tenant *tenant);
struct tenant *tenant_database_find_by_name(char *name);
struct tenant *tenant_database_find_by_socket_descriptor(int socket_descriptor);
struct tenant *tenant_database_find_by_port(uint16_t port);
struct tenant *tenant_database_find_by_ptr(void *ptr);

typedef void (*tenant_database_foreach_cb_t)(struct tenant *, void *, void *);
void tenant_database_foreach(tenant_database_foreach_cb_t, void *, void *);

static inline int
tenant_policy_specific_init(struct tenant *tenant, struct tenant_config *config)
{
	switch (scheduler) {
	case SCHEDULER_FIFO:
		break;
	case SCHEDULER_EDF:
		break;
	case SCHEDULER_MTDS:
		/* Deferable Server Initialization */
		tenant->replenishment_period = (uint64_t)config->replenishment_period_us * runtime_processor_speed_MHz;
		tenant->max_budget           = (uint64_t)config->max_budget_us * runtime_processor_speed_MHz;
		tenant->remaining_budget     = tenant->max_budget;

		tenant->pwt_sandboxes = (struct perworker_tenant_sandbox_queue *)malloc(
		  runtime_worker_threads_count * sizeof(struct perworker_tenant_sandbox_queue));
		if (!tenant->pwt_sandboxes) {
			fprintf(stderr, "Failed to allocate tenant_sandboxes array: %s\n", strerror(errno));
			return -1;
		};

		memset(tenant->pwt_sandboxes, 0,
		       runtime_worker_threads_count * sizeof(struct perworker_tenant_sandbox_queue));

		for (int i = 0; i < runtime_worker_threads_count; i++) {
			tenant->pwt_sandboxes[i].sandboxes = priority_queue_initialize(RUNTIME_TENANT_QUEUE_SIZE, false,
			                                                               sandbox_get_priority);
			tenant->pwt_sandboxes[i].tenant    = tenant;
			tenant->pwt_sandboxes[i].mt_class  = (tenant->replenishment_period == 0) ? MT_DEFAULT
			                                                                         : MT_GUARANTEED;
			tenant->pwt_sandboxes[i].tenant_timeout.tenant = tenant;
			tenant->pwt_sandboxes[i].tenant_timeout.pwt    = &tenant->pwt_sandboxes[i];
		}

		/* Initialize the tenant's global request queue */
		tenant->tgrq_requests                   = malloc(sizeof(struct tenant_global_request_queue));
		tenant->tgrq_requests->sandbox_requests = priority_queue_initialize(RUNTIME_TENANT_QUEUE_SIZE, false,
		                                                                    sandbox_get_priority);
		tenant->tgrq_requests->tenant           = tenant;
		tenant->tgrq_requests->mt_class = (tenant->replenishment_period == 0) ? MT_DEFAULT : MT_GUARANTEED;
		tenant->tgrq_requests->tenant_timeout.tenant = tenant;
		tenant->tgrq_requests->tenant_timeout.pwt    = NULL;
		break;

	case SCHEDULER_MTDBF:
		break;
	}

	return 0;
}

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
	map_init(&tenant->scratch_storage);

	/* Deferrable Server init */
	tenant_policy_specific_init(tenant, config);

	for (int i = 0; i < config->routes_len; i++) {
		struct module *module = module_database_find_by_path(&tenant->module_db, config->routes[i].path);
		if (module == NULL) {
			/* Ownership of path moves here */
			module = module_alloc(config->routes[i].path);
			if (module != NULL) {
				module_database_add(&tenant->module_db, module);
				config->routes[i].path = NULL;
			}
		} else {
			free(config->routes[i].path);
			config->routes[i].path = NULL;
		}

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
 * Get Timeout priority for Priority Queue ordering
 * @param element tenant_timeout
 * @returns the priority of the tenant _timeout element
 */
static inline uint64_t
tenant_timeout_get_priority(void *element)
{
	return ((struct tenant_timeout *)element)->timeout;
}

/**
 * Compute the next timeout given a tenant's replenishment period
 * @param m_replenishment_period
 * @return given tenant's next timeout
 */
static inline uint64_t
get_next_timeout_of_tenant(uint64_t replenishment_period)
{
	assert(replenishment_period != 0);
	uint64_t now = __getcycles();
	return runtime_boot_timestamp
	       + ((now - runtime_boot_timestamp) / replenishment_period + 1) * replenishment_period;
}


/**
 * Start the tenant as a server listening at tenant->port
 * @param tenant
 * @returns 0 on success, -1 on error
 */
int tenant_listen(struct tenant *tenant);
int listener_thread_register_tenant(struct tenant *tenant);
