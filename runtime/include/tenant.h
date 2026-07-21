#pragma once

#include "epoll_tag.h"
#include "http_router.h"
#include "map.h"
#include "module_database.h"
#include "tcp_server.h"

struct tenant_wrapper {
	struct priority_queue *sandboxes;
	struct priority_queue *destination_queue;
	struct tenant         *tenant; // to be able to find the RB/MB/RP/RT.
	uint64_t               ds_timeout;
} CACHE_PAD_ALIGNED;

struct job_node {
	struct ps_list           list;
	uint64_t                 exec;
	uint64_t                 timestamp;
	struct sandbox_metadata *sandbox_meta;
};

struct tenant_reservation_server {
	uint64_t            max_budget_guaranteed;
	uint64_t            budget_guaranteed;
	uint64_t            budget_best;
	struct ps_list_head admitted_jobs_list;
	struct ps_list_head admitted_BE_jobs_list;
};

struct tenant {
	enum epoll_tag         tag; /* Tag must be first member */
	char                  *name;
	struct tcp_server      tcp_server;
	http_router_t          router;
	struct module_database module_db;
	struct map             scratch_storage;

	/* Multi-Tenancy Attributes */
	uint64_t                 max_relative_deadline;
	uint64_t                 replenishment_period;      /* cycles, not changing after init */
	uint64_t                 max_budget;                /* cycles, not changing after init */
	uint64_t                 defferable_server_timeout; /* cycles, next replenishment time in the future */
	_Atomic volatile int64_t remaining_budget;          /* cycles left till next replenishment, can be negative */
	uint8_t                  reservation_percentile;    /* percentile of the overall reservation utilization */

	struct tenant_wrapper           *tw_wrappers;
	struct tenant_wrapper           *global_tenant_wrapper;
	struct priority_queue           *local_sandbox_metas, *global_sandbox_metas;
	struct tenant_reservation_server trs;
	uint32_t                         num_of_overshooted_sandboxes;
	uint8_t                          max_overshoot_of_same_sandbox;
};


/**
 * Check whether a tenant has remaining budget
 * @param tenant tenant
 * @returns true if the tenant has remaining budget, false otherwise
 */
static inline uint64_t
tenant_has_budget(struct tenant *tenant)
{
	return tenant->remaining_budget > 0;
}
