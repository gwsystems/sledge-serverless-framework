#include <errno.h>

#include "panic.h"
#include "runtime.h"
#include "tenant.h"
#include "panic.h"
#include "tenant_functions.h"

/*******************
 * Tenant Database *
 ******************/

struct tenant *tenant_database[RUNTIME_MAX_TENANT_COUNT] = {NULL};
size_t         tenant_database_count                     = 0;

/**
 * Adds a tenant to the in-memory tenant DB
 * @param tenant tenant to add
 * @return 0 on success. -ENOSPC when full
 */
int
tenant_database_add(struct tenant *tenant)
{
	assert(tenant_database_count <= RUNTIME_MAX_TENANT_COUNT);

	int rc;

	if (tenant_database_count == RUNTIME_MAX_TENANT_COUNT) goto err_no_space;
	tenant_database[tenant_database_count++] = tenant;

	rc = 0;
done:
	return rc;
err_no_space:
	panic("Cannot add tenant. Database is full.\n");
	rc = -ENOSPC;
	goto done;
}

/**
 * Given a name, find the associated tenant
 * @param name
 * @return tenant or NULL if no match found
 */
struct tenant *
tenant_database_find_by_name(char *name)
{
	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		if (strcmp(tenant_database[i]->name, name) == 0) return tenant_database[i];
	}
	return NULL;
}

/**
 * Given a socket_descriptor, find the associated tenant
 * @param socket_descriptor
 * @return tenant or NULL if no match found
 */
struct tenant *
tenant_database_find_by_socket_descriptor(int socket_descriptor)
{
	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		if (tenant_database[i]->tcp_server.socket_descriptor == socket_descriptor) return tenant_database[i];
	}
	return NULL;
}

/**
 * Given a port, find the associated tenant
 * @param port
 * @return tenant or NULL if no match found
 */
struct tenant *
tenant_database_find_by_port(uint16_t port)
{
	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		if (tenant_database[i]->tcp_server.port == port) return tenant_database[i];
	}
	return NULL;
}

/**
 * Checks is an opaque pointer is a tenant by comparing against
 */
struct tenant *
tenant_database_find_by_ptr(void *ptr)
{
	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		if (tenant_database[i] == ptr) return tenant_database[i];
	}
	return NULL;
}

void
tenant_database_foreach(void (*cb)(struct tenant *, void *), void *arg)
{
	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		cb(tenant_database[i], arg);
	}
}

#ifdef TRAFFIC_CONTROL
void
tenant_database_init_reservations()
{
	printf("Runtime Max Deadline: %lu\n", runtime_max_deadline);

	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		struct tenant *t = tenant_database[i];
		if (t->tcp_server.port == 55555) continue;

		t->trs.max_budget_guaranteed = REPLENISHMENT_PERIOD * runtime_worker_threads_count * t->reservation_percentile / 100;
		t->trs.budget_guaranteed = t->trs.max_budget_guaranteed;
		t->trs.budget_best = UINT64_MAX;

		printf("Tenant %s, max_deadline: %lu, max_budget: %ld\n", t->name, t->max_relative_deadline, t->trs.max_budget_guaranteed);
	}
}

/**
 * @brief Iterate through the tenant databse and print DBF info per tenant
 */
void
tenant_database_print_reservations()
{
	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		struct tenant *t = tenant_database[i];
		if (t->tcp_server.port == 55555) continue;

		printf("\nTENANT: %s INFO:\n", t->name);
		printf("Global_meta_size: %d, Local_meta_size: %d\n", priority_queue_length_nolock(t->global_sandbox_metas), priority_queue_length_nolock(t->local_sandbox_metas));
		printf("Number of total overshoots: %u, MAX of overshoots from the same sandbox: %u \n", t->num_of_overshooted_sandboxes, t->max_overshoot_of_same_sandbox);
		// printf("Best Effort demands: %lu\n", t->trs.best_effort_cycles);
		// dbf_print(t->tenant_dbf, __getcycles());
		
		tenant_print_jobs(t);
	}
}

/**
 * Checks is an opaque pointer is a tenant by comparing against
 */
struct tenant *
tenant_database_find_tenant_most_oversupply(struct tenant *tenant_to_exclude, uint64_t time_of_oversupply, bool weak_shed, struct sandbox_metadata **sandbox_meta_to_remove)
{
	assert(sandbox_meta_to_remove != NULL);
	struct tenant *tenant_to_punish    = NULL;
	uint64_t       min_tenant_BE_budget = UINT64_MAX;

	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		// if (tenant_database[i] == tenant_to_exclude) continue;
		struct tenant *tenant = tenant_database[i];
		assert(tenant);
		if (tenant->tcp_server.port == 55555) continue;
		if (priority_queue_length_nolock(tenant->global_sandbox_metas) + priority_queue_length_nolock(tenant->local_sandbox_metas) == 0) continue;

		struct sandbox_metadata *sandbox_meta = NULL;
		int rc = priority_queue_top_nolock(tenant->global_sandbox_metas, (void **)&sandbox_meta);

		if (!sandbox_meta || sandbox_meta->absolute_deadline > time_of_oversupply) {
			sandbox_meta = NULL;
			rc = priority_queue_top_nolock(tenant->local_sandbox_metas, (void **)&sandbox_meta);
		}
		if (!sandbox_meta || sandbox_meta->absolute_deadline > time_of_oversupply) continue;
		assert(rc == 0);
		assert(sandbox_meta);

		if (tenant->trs.budget_best < min_tenant_BE_budget) {
			min_tenant_BE_budget = tenant->trs.budget_best;
			tenant_to_punish     = tenant;
			*sandbox_meta_to_remove = sandbox_meta;
		}
	}

/*
	if(weak_shed && tenant_to_punish == NULL) printf("Weak mode: No tenant to punish for %s\n", tenant_to_exclude->name);
	if(!weak_shed && tenant_to_punish == NULL) printf("Strong mode: No tenant to punish for %s\n", tenant_to_exclude->name);

	if (weak_shed && tenant_to_punish) {
		assert(tenant_to_punish->reservation_percentile==20);
		printf("MODE weak=%u - pending tenant %s \n", weak_shed, tenant_to_exclude->name);
		printf("Start (ms): %lu\n", start_time);
		printf("AbsDL (ms): %lu\n", absolute_deadline);
		printf("ToS   (ms): %lu\n", time_of_oversupply);
		printf("RelativeDL (ms): %lu\n", absolute_deadline/runtime_quantum - start_time/runtime_quantum);
		
		min_tenant_BE_budget = UINT64_MAX;
		for (size_t i = 0; i < tenant_database_count; i++) {
			assert(tenant_database[i]);
			struct tenant *tenant = tenant_database[i];
			assert(tenant);
			if (tenant->tcp_server.port == 55555) continue;

			uint64_t t_budget_BE = tenant->trs.budget_BE;
			if (t_budget_BE < min_tenant_BE_budget) {
				min_tenant_BE_budget = t_budget_BE;
				tenant_to_punish     = tenant;
			}

			printf("Tenant: %s, rp=%u, min_tenant_BE_budget=%lu\n", tenant->name,
			       tenant->reservation_percentile, min_tenant_BE_budget);
			tenant_print_jobs(tenant);
			printf("Tenant Globl SIZE: %d\n", priority_queue_length_nolock(tenant->global_sandbox_metas));
			printf("Tenant Local SIZE: %d\n\n", priority_queue_length_nolock(tenant->local_sandbox_metas));
		}

		const int N_VIRT_WORKERS_DBF = USING_AGGREGATED_GLOBAL_DBF ? 1 : runtime_worker_threads_count;
		for (int i = 0; i < N_VIRT_WORKERS_DBF; i++) {
			printf("GL Worker #%d\n", i);
			dbf_print(global_virt_worker_dbfs[i], start_time);
		}
		assert(0);
	}*/
	assert((*sandbox_meta_to_remove)==NULL || (*sandbox_meta_to_remove)->tenant == tenant_to_punish);
	return tenant_to_punish;
}

void
tenant_database_replenish_all()
{
	const uint64_t now = __getcycles();

	for (size_t i = 0; i < tenant_database_count; i++) {
		assert(tenant_database[i]);
		struct tenant *t = tenant_database[i];
		if (t->tcp_server.port == 55555) continue;

		tenant_replenish(t, now);
	}
}

#endif