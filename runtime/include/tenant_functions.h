#pragma once

#include <stdint.h>
#include <string.h>

#include "http.h"
#include "listener_thread.h"
#include "module_database.h"
#include "panic.h"
#include "priority_queue.h"
#include "sandbox_functions.h"
#include "scheduler_options.h"
#include "tenant.h"
#include "tenant_config.h"
#include "priority_queue.h"
#include "sandbox_functions.h"
#include "dbf.h"
#include "global_request_scheduler_mtds.h"

// #define REPLENISHMENT_PERIOD (tenant->max_relative_deadline * 10)
// #define REPLENISHMENT_PERIOD (runtime_max_deadline)
#define REPLENISHMENT_PERIOD (1 * 1000000UL * runtime_processor_speed_MHz) /* 2 second */

int tenant_listen(struct tenant *tenant);
int listener_thread_register_tenant(struct tenant *tenant);
void tenant_preprocess(struct http_session *session);

int            tenant_database_add(struct tenant *tenant);
struct tenant *tenant_database_find_by_name(char *name);
struct tenant *tenant_database_find_by_socket_descriptor(int socket_descriptor);
struct tenant *tenant_database_find_by_port(uint16_t port);
struct tenant *tenant_database_find_by_ptr(void *ptr);
void           tenant_database_print_reservations();
void           tenant_database_init_reservations();
void           tenant_database_clear_pools();
void           tenant_database_replenish_all();
struct tenant *tenant_database_find_tenant_most_oversupply(struct tenant *tenant_to_exclude, uint64_t time_of_oversupply, bool weak_shed, struct sandbox_metadata **sandbox_meta_to_remove);

typedef void (*tenant_database_foreach_cb_t)(struct tenant *, void *);
void         tenant_database_foreach(tenant_database_foreach_cb_t, void *);

static inline uint64_t
sandbox_meta_get_priority(void *element)
{
	struct sandbox_metadata *sandbox_meta = (struct sandbox_metadata *)element;
	return sandbox_meta->absolute_deadline;
}

static inline void
tenant_policy_specific_init(struct tenant *tenant, struct tenant_config *config)
{
	/* Skip the Admin Tenant */
	if (tenant->tcp_server.port == 55555) return;

	switch (scheduler) {
	case SCHEDULER_FIFO:
		break;
	case SCHEDULER_EDF:
	case SCHEDULER_SJF:
		break;
	case SCHEDULER_MTDS: {
		/* Deferrable Server Initialization */
		tenant->replenishment_period = (uint64_t)config->replenishment_period_us * runtime_processor_speed_MHz;
		tenant->max_budget           = (uint64_t)config->max_budget_us * runtime_processor_speed_MHz;
		tenant->remaining_budget     = tenant->max_budget;

		config->replenishment_period_us = 0;
		config->max_budget_us           = 0;

		tenant->tw_wrappers = (struct tenant_wrapper *)
		  calloc(runtime_worker_threads_count, sizeof(struct tenant_wrapper));
		assert(tenant->tw_wrappers);

		for (int i = 0; i < runtime_worker_threads_count; i++) {
			tenant->tw_wrappers[i].sandboxes = priority_queue_initialize(RUNTIME_TENANT_QUEUE_SIZE, false,
			                                                               sandbox_get_priority);
			tenant->tw_wrappers[i].tenant    = tenant;
			tenant->tw_wrappers[i].destination_queue = NULL;
		}

		/* Initialize the tenant's global request queue */
		tenant->global_tenant_wrapper = calloc(1, sizeof(struct tenant_wrapper));
		assert(tenant->global_tenant_wrapper);

		tenant->global_tenant_wrapper->sandboxes = priority_queue_initialize(RUNTIME_TENANT_QUEUE_SIZE, false,
		                                                                    sandbox_get_priority);
		tenant->global_tenant_wrapper->tenant           = tenant;
		tenant->global_tenant_wrapper->destination_queue = NULL;
		break;
	}
	case SCHEDULER_MTDBF:
		tenant->reservation_percentile = config->reservation_percentile;
		config->reservation_percentile = 0;

		ps_list_head_init(&tenant->trs.admitted_jobs_list);
		ps_list_head_init(&tenant->trs.admitted_BE_jobs_list);

		tenant->local_sandbox_metas =
		  priority_queue_initialize_new(RUNTIME_RUNQUEUE_SIZE, false, sandbox_meta_get_priority, NULL,
		                                local_sandbox_meta_update_pq_idx_in_tenant_queue);
		tenant->global_sandbox_metas =
		  priority_queue_initialize_new(RUNTIME_RUNQUEUE_SIZE, false, sandbox_meta_get_priority, NULL,
		                                local_sandbox_meta_update_pq_idx_in_tenant_queue);
		break;
	}
}

static inline struct tenant *
tenant_alloc(struct tenant_config *config)
{
	struct tenant *existing_tenant = tenant_database_find_by_name(config->name);
	if (existing_tenant != NULL) panic("Tenant %s is already initialized\n", existing_tenant->name);

	existing_tenant = tenant_database_find_by_port(config->port);
	if (existing_tenant != NULL) {
		panic("Tenant %s is already configured with port %u\n", existing_tenant->name, config->port);
	}

	struct tenant *tenant = (struct tenant *)calloc(1, sizeof(struct tenant));

	/* Move name */
	tenant->tag  = EPOLL_TAG_TENANT_SERVER_SOCKET;
	tenant->name = config->name;
	config->name = NULL;

	tenant->max_relative_deadline = (uint64_t)config->max_relative_deadline_us * runtime_processor_speed_MHz;

	tcp_server_init(&tenant->tcp_server, config->port);
	http_router_init(&tenant->router, config->routes_len);
	module_database_init(&tenant->module_db);
	map_init(&tenant->scratch_storage);

	/* Scheduling Policy specific tenant init */
	tenant_policy_specific_init(tenant, config);

	for (int i = 0; i < config->routes_len; i++) {
		struct module *module = module_database_find_by_path(&tenant->module_db, config->routes[i].path);
		if (module == NULL) {
			/* Ownership of path moves here */
			module = module_alloc(config->routes[i].path, APP_MODULE);
			if (module != NULL) {
				module_database_add(&tenant->module_db, module);
				config->routes[i].path = NULL;
			}
		} else {
			free(config->routes[i].path);
			config->routes[i].path = NULL;
		}

		assert(module != NULL);

		struct module *module_proprocess = NULL;

#ifdef EXECUTION_REGRESSION
		if (config->routes[i].path_preprocess) {
			module_proprocess = module_database_find_by_path(&tenant->module_db,
			                                                 config->routes[i].path_preprocess);
			if (module_proprocess == NULL) {
				/* Ownership of path moves here */
				module_proprocess = module_alloc(config->routes[i].path_preprocess, PREPROCESS_MODULE);
				if (module_proprocess != NULL) {
					module_database_add(&tenant->module_db, module_proprocess);
					config->routes[i].path_preprocess = NULL;
				}
			} else {
				free(config->routes[i].path_preprocess);
				config->routes[i].path_preprocess = NULL;
			}

			assert(module_proprocess != NULL);
		}
#endif

		/* Ownership of config's route and http_resp_content_type move here */
		int rc = http_router_add_route(&tenant->router, &config->routes[i], module, module_proprocess);
		if (unlikely(rc != 0)) {
			panic("Tenant %s defined %lu routes, but router failed to grow beyond %lu\n", tenant->name,
			      config->routes_len, tenant->router.capacity);
		}

		config->routes[i].route                  = NULL;
		config->routes[i].http_resp_content_type = NULL;
	}

	return tenant;
}

#ifdef TRAFFIC_CONTROL

static void
tenant_print_jobs(struct tenant *tenant)
{
	struct tenant_reservation_server *trs  = &tenant->trs;
	struct job_node                  *head = NULL;

	int i = 0;
	printf("\nTenant Guaranteed Budget: %lu/%lu\n", trs->budget_guaranteed, trs->max_budget_guaranteed);
	ps_list_foreach_d(&trs->admitted_jobs_list, head)
	{
		printf("GR-Job #%d: Arrival:%lu, Replenishment_at: %lu\t Exec:%lu\n", i++, head->timestamp, head->timestamp + REPLENISHMENT_PERIOD, head->exec);
	}

	i = 0;
	printf("\nTenant Best Effort Budget: %lu/%lu\n", trs->budget_best, UINT64_MAX);
	ps_list_foreach_d(&trs->admitted_BE_jobs_list, head)
	{
		printf("BE-Job #%d: Arrival:%lu\t Exec:%lu\n", i++, head->timestamp, head->exec);
	}
}

static void
tenant_replenish(struct tenant *tenant, uint64_t now)
{
	struct tenant_reservation_server *trs  = &tenant->trs;
	struct job_node                  *head = ps_list_head_first_d(&trs->admitted_jobs_list, struct job_node);

	while (!ps_list_is_head_d(&trs->admitted_jobs_list, head)) {
		assert(now > head->timestamp);
		if (now - head->timestamp < REPLENISHMENT_PERIOD) break;

		struct job_node *tmp_next = ps_list_next_d(head);

		trs->budget_guaranteed += head->exec;

		if (head->sandbox_meta) {
			assert(head->sandbox_meta->trs_job_node);
			head->sandbox_meta->trs_job_node = NULL;
		}

		ps_list_rem_d(head);
		free(head);

		head = tmp_next;
	}

	assert(trs->budget_guaranteed <= trs->max_budget_guaranteed);

	head = ps_list_head_first_d(&trs->admitted_BE_jobs_list, struct job_node);

	while (!ps_list_is_head_d(&trs->admitted_BE_jobs_list, head)) {
		assert(now >= head->timestamp);
		if (now - head->timestamp < REPLENISHMENT_PERIOD) break;

		struct job_node *tmp_next = ps_list_next_d(head);

		trs->budget_best += head->exec;

		ps_list_rem_d(head);
		free(head);

		head = tmp_next;
	}
}

static inline bool
tenant_can_admit_guaranteed(struct tenant *tenant, uint64_t now, uint64_t adjustment)
{
	// tenant_replenish(tenant, now);
	return tenant->trs.budget_guaranteed >= adjustment;
}

static bool
tenant_try_add_job_as_guaranteed(struct tenant *tenant, uint64_t arrival_time, uint64_t adjustment, struct sandbox_metadata *sandbox_meta)
{
	assert(adjustment > 0);

	struct tenant_reservation_server *trs = &tenant->trs;
	if (trs->budget_guaranteed < adjustment) return false;

	assert(sandbox_meta->trs_job_node == NULL);
	assert(sandbox_meta);
	assert(sandbox_meta->terminated == false);
	
	struct job_node *new_node = (struct job_node *)malloc(sizeof(struct job_node));
	ps_list_init_d(new_node);
	new_node->exec         = adjustment;
	new_node->timestamp    = arrival_time;
	new_node->sandbox_meta = sandbox_meta;

	assert(ps_list_singleton_d(new_node));
	struct job_node *tail = ps_list_head_last_d(&trs->admitted_jobs_list, struct job_node);
	ps_list_add_d(tail, new_node);
	sandbox_meta->trs_job_node = new_node;

	assert(trs->budget_guaranteed >= adjustment);
	trs->budget_guaranteed -= adjustment;
	return true;
}

static void
tenant_force_add_job_as_best(struct tenant *tenant, uint64_t arrival_time, uint64_t adjustment)
{
	assert(adjustment > 0);

	struct tenant_reservation_server *trs = &tenant->trs;
	struct job_node *new_node = (struct job_node *)malloc(sizeof(struct job_node));
	ps_list_init_d(new_node);
	new_node->exec      = adjustment;
	new_node->timestamp = arrival_time;

	assert(ps_list_singleton_d(new_node));
	struct job_node *tail = ps_list_head_last_d(&trs->admitted_BE_jobs_list, struct job_node);
	ps_list_add_d(tail, new_node);

	assert(trs->budget_best > adjustment);
	trs->budget_best -= adjustment;
}

static void
tenant_reduce_guaranteed_job_demand(struct tenant *tenant, uint64_t adjustment, struct sandbox_metadata *sandbox_meta)
{
	assert(sandbox_meta);
	assert(sandbox_meta->global_queue_type == 1);
	assert(sandbox_meta->trs_job_node);

	struct tenant_reservation_server *trs  = &tenant->trs;
	struct job_node                  *node = sandbox_meta->trs_job_node;
	assert(node->sandbox_meta == sandbox_meta);

	assert(node->exec >= adjustment);
	node->exec -= adjustment;
	trs->budget_guaranteed += adjustment;

	sandbox_meta->trs_job_node->sandbox_meta = NULL;
	sandbox_meta->trs_job_node = NULL;
}

#endif