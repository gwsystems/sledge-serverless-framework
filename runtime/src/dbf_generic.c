#include <string.h>
#include <assert.h>
#include "dbf.h"

static struct dbf_config dbf_conf;
// void *global_dbf_temp;

int
dbf_get_worker_idx(void *dbf)
{
	assert(dbf_conf.get_worker_idx_fn != NULL);
	return dbf_conf.get_worker_idx_fn(dbf);
}

// uint64_t
// dbf_get_max_relative_dl(void *dbf)
// {
// 	assert(dbf_conf.get_max_relative_dl_fn != NULL);
// 	return dbf_conf.get_max_relative_dl_fn(dbf);
// }

uint64_t
dbf_get_time_of_oversupply(void *dbf)
{
	assert(dbf_conf.get_time_of_oversupply_fn != NULL);
	return dbf_conf.get_time_of_oversupply_fn(dbf);
}

void
dbf_print(void *dbf, uint64_t start_time)
{
	assert(dbf_conf.print_fn != NULL);
	return dbf_conf.print_fn(dbf, start_time);
}

// void *
// dbf_grow(void *dbf, uint64_t new_max_relative_deadline)
// {
// 	assert(dbf_conf.grow_fn != NULL);
// 	return dbf_conf.grow_fn(dbf, new_max_relative_deadline);
// }

bool
dbf_try_update_demand(void *dbf, uint64_t start_time, uint64_t route_relative_deadline, uint64_t abs_deadline,
                      uint64_t adjustment, dbf_update_mode_t dbf_update_mode, void *new_message, struct sandbox_metadata *sandbox_meta)
{
	assert(dbf_conf.try_update_demand_fn != NULL);
	return dbf_conf.try_update_demand_fn(dbf, start_time, route_relative_deadline, abs_deadline, adjustment,
	                                     dbf_update_mode, new_message, sandbox_meta);
}

uint64_t
dbf_get_demand_overgone_its_supply_at(void *dbf, uint64_t start_time, uint64_t abs_deadline, uint64_t time_of_oversupply)
{
	assert(dbf_conf.get_demand_overgone_its_supply_at_fn != NULL);
	return dbf_conf.get_demand_overgone_its_supply_at_fn(dbf, start_time, abs_deadline, time_of_oversupply);
}

void
dbf_free(void *dbf)
{
	assert(dbf_conf.free_fn != NULL);
	return dbf_conf.free_fn(dbf);
}

void
dbf_plug_functions(struct dbf_config *config)
{
	memcpy(&dbf_conf, config, sizeof(struct dbf_config));
}

void 
*dbf_initialize(uint32_t num_of_workers, uint8_t reservation_percentile, int worker_idx, struct tenant *tenant)
{
#ifdef DBF_USE_LINKEDLIST
	return dbf_list_initialize(num_of_workers, reservation_percentile, worker_idx, tenant);
#else
	return dbf_array_initialize(num_of_workers, reservation_percentile, worker_idx, tenant);
#endif
}
