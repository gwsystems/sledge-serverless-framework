#pragma once

#include <stdlib.h>
#include "tenant.h"
#include "message.h"

#define DBF_USE_LINKEDLIST
// static const bool USING_AGGREGATED_GLOBAL_DBF = true;

/* Returns pointer back if successful, null otherwise */
// extern void *global_dbf;
extern void **global_virt_worker_dbfs;
extern void *global_worker_dbf;

struct demand_node {
	struct ps_list list;
	uint64_t abs_deadline;
	uint64_t demand;
	// uint64_t demand_sum;
	// struct sandbox_metadata *sandbox_meta;
	struct tenant *tenant;
};

typedef enum dbf_update_mode
{
	DBF_CHECK_AND_ADD_DEMAND,               /* normal mode for adding new sandbox demands */
	DBF_FORCE_ADD_NEW_SANDBOX_DEMAND,       /* work-conservation mode*/
	DBF_FORCE_ADD_MANUAL_DEMAND,            /* work-conservation mode*/
	DBF_REDUCE_EXISTING_DEMAND,             /* normal mode for reducing existing sandbox demands */
	// DBF_CHECK_EXISTING_SANDBOX_EXTRA_DEMAND, /* special case when a sandbox goes over its expected exec */
	DBF_DELETE_EXISTING_DEMAND             /* normal mode for removing existing sandbox demand */
} dbf_update_mode_t;

typedef int (*dbf_get_worker_idx_fn_t)(void *);
typedef uint64_t (*dbf_get_time_of_oversupply_fn_t)(void *);
typedef void (*dbf_print_fn_t)(void *, uint64_t);
typedef bool (*dbf_try_update_demand_fn_t)(void *, uint64_t, uint64_t, uint64_t, uint64_t, dbf_update_mode_t, void *, struct sandbox_metadata *sandbox_meta);
typedef uint64_t (*dbf_get_demand_overgone_its_supply_at_fn_t)(void *, uint64_t, uint64_t, uint64_t);
typedef void (*dbf_free_fn_t)(void *);

struct dbf_config {
	dbf_get_worker_idx_fn_t                    get_worker_idx_fn;
	// dbf_get_max_relative_dl_fn_t               get_max_relative_dl_fn;
	dbf_get_time_of_oversupply_fn_t            get_time_of_oversupply_fn;
	dbf_print_fn_t                             print_fn;
	// dbf_grow_fn_t                              grow_fn;
	dbf_try_update_demand_fn_t                 try_update_demand_fn;
	dbf_get_demand_overgone_its_supply_at_fn_t get_demand_overgone_its_supply_at_fn;
	dbf_free_fn_t                              free_fn;
};

int      dbf_get_worker_idx(void *);
// uint64_t dbf_get_max_relative_dl(void *);
uint64_t dbf_get_time_of_oversupply(void *);
void     dbf_print(void *, uint64_t);
// void    *dbf_grow(void *, uint64_t);
bool     dbf_try_update_demand(void *, uint64_t, uint64_t, uint64_t, uint64_t, dbf_update_mode_t, void *, struct sandbox_metadata *sandbox_meta);
uint64_t dbf_get_demand_overgone_its_supply_at(void *, uint64_t, uint64_t, uint64_t);
void     dbf_free(void *);

void dbf_plug_functions(struct dbf_config *config);

void *dbf_list_initialize(uint32_t, uint8_t, int, struct tenant *);
void *dbf_array_initialize(uint32_t, uint8_t, int, struct tenant *);
void *dbf_initialize(uint32_t num_of_workers, uint8_t reservation_percentile, int worker_idx, struct tenant *tenant);


bool
dbf_list_try_add_new_demand(void *dbf_raw, uint64_t start_time, uint64_t abs_deadline, uint64_t adjustment, struct sandbox_metadata *sm);

void
dbf_list_force_add_extra_slack(void *dbf_raw, struct sandbox_metadata *sm, uint64_t adjustment);

void
dbf_list_reduce_demand(struct sandbox_metadata *sm, uint64_t adjustment, bool delete_node);