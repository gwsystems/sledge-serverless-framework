#pragma once

#include "http_router.h"
#include "map.h"
#include "module_database.h"
#include "tcp_server.h"

enum MULTI_TENANCY_CLASS
{
	MT_DEFAULT,
	MT_GUARANTEED
};

struct tenant_timeout {
	uint64_t                               timeout;
	struct tenant                         *tenant;
	struct perworker_tenant_sandbox_queue *pwt;
};

struct perworker_tenant_sandbox_queue {
	struct priority_queue   *sandboxes;
	struct tenant           *tenant; // to be able to find the RB/MB/RP/RT.
	struct tenant_timeout    tenant_timeout;
	enum MULTI_TENANCY_CLASS mt_class; // check whether the corresponding PWM has been demoted
} __attribute__((aligned(CACHE_PAD)));

struct tenant_global_request_queue {
	struct priority_queue                    *sandbox_requests;
	struct tenant                            *tenant;
	struct tenant_timeout                     tenant_timeout;
	_Atomic volatile enum MULTI_TENANCY_CLASS mt_class;
};

struct tenant {
	char                  *name;
	struct tcp_server      tcp_server;
	http_router_t          router;
	struct module_database module_db;
	struct map             scratch_storage;

	/* Deferrable Server Attributes */
	uint64_t                 replenishment_period; /* cycles, not changing after init */
	uint64_t                 max_budget;           /* cycles, not changing after init */
	_Atomic volatile int64_t remaining_budget;     /* cycles left till next replenishment, can be negative */

	struct perworker_tenant_sandbox_queue *pwt_sandboxes;
	struct tenant_global_request_queue    *tgrq_requests;
};


/**
 * Check whether a tenant is a paid tenant
 * @param tenant tenant
 * @returns true if the tenant is paid, false otherwise
 */
static inline uint64_t
tenant_is_paid(struct tenant *tenant)
{
	return tenant->replenishment_period > 0;
}
