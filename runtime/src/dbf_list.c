#include <assert.h>
#include "dbf.h"
#include "sandbox_types.h"

struct dbf_list {
	struct tenant *tenant;
	int      worker_idx;
	uint64_t max_relative_deadline;
	double base_supply; /* supply amount for time 1 */
	uint64_t time_of_oversupply;
	uint64_t demand_total;

	struct ps_list_head demands_list;
};

static inline int 
dbf_list_get_worker_idx(void * dbf_raw)
{
	assert(dbf_raw);
	struct dbf_list *dbf = (struct dbf_list *)dbf_raw;
	return dbf->worker_idx;
}

/*static inline uint64_t
dbf_list_get_max_relative_dl(void * dbf_raw)
{
	assert(dbf_raw);
	struct dbf_list *dbf = (struct dbf_list *)dbf_raw;
	return dbf->max_relative_deadline;
}*/

static inline uint64_t
dbf_list_get_time_of_oversupply(void * dbf_raw)
{
	assert(dbf_raw);
	struct dbf_list *dbf = (struct dbf_list *)dbf_raw;
	return dbf->time_of_oversupply;
}

static void
dbf_list_print(void *dbf_raw, uint64_t start_time)
{
	assert(dbf_raw != NULL);
	struct dbf_list *dbf = (struct dbf_list *)dbf_raw;

	printf("DBF INFO LL:\n\
	\t WorkerIDX: \t%d\n\
	\t Basic Supply: \t%lf\n\n", dbf->worker_idx, dbf->base_supply);

	struct demand_node *node = NULL;
	uint64_t demand_sum = 0;

	ps_list_foreach_d(&dbf->demands_list, node) 
	{
		const uint32_t live_deadline_len = node->abs_deadline - start_time;
		const uint64_t max_supply_at_time_i = live_deadline_len * dbf->base_supply;
		demand_sum += node->demand;
		uint64_t over = 0;
		if (demand_sum >= max_supply_at_time_i) over = demand_sum - max_supply_at_time_i;
		printf("demand_at[%lu] = %lu, t=%s, demand_sum=%lu/supply=%lu, demand_over=%lu\n", node->abs_deadline, node->demand, node->tenant->name, demand_sum, max_supply_at_time_i, over);
	}
}

bool
dbf_list_try_add_new_demand(void *dbf_raw, uint64_t start_time, uint64_t abs_deadline, uint64_t adjustment, struct sandbox_metadata *sm)
{
	assert(dbf_raw != NULL);
	assert(start_time < abs_deadline);
	assert(sm);
	assert(sm->demand_node == NULL);
	assert(adjustment > 0);
	// if (adjustment == 0) return false;

	struct dbf_list *dbf = (struct dbf_list *)dbf_raw;
	struct demand_node *node = NULL;
	uint64_t past_deadline_demand = 0;
	uint64_t demand_sum = 0;

	ps_list_foreach_d(&dbf->demands_list, node) 
	{
		if (node->abs_deadline <= start_time) past_deadline_demand = demand_sum;
		else if (node->abs_deadline >= abs_deadline) break;
		demand_sum += node->demand;
	}

	struct demand_node *node_spot = node;
	assert(abs_deadline != node->abs_deadline);
	assert(abs_deadline == sm->absolute_deadline);

	demand_sum += adjustment;
	const uint64_t live_deadline_len = abs_deadline - start_time;
	const uint64_t max_supply_at_time_i = live_deadline_len * dbf->base_supply; // + past_deadline_demand;
	if (demand_sum > max_supply_at_time_i) {
		dbf->time_of_oversupply = abs_deadline;
		goto err;
	}

	while(!ps_list_is_head_d(&dbf->demands_list, node)) {
		struct demand_node *tmp_next = ps_list_next_d(node);
		const uint64_t live_deadline_len = node->abs_deadline - start_time;
		const uint64_t max_supply_at_time_i = live_deadline_len * dbf->base_supply; // + past_deadline_demand;
		demand_sum += node->demand;
		if (demand_sum > max_supply_at_time_i) {
			dbf->time_of_oversupply = node->abs_deadline;
			goto err;
		}
		node = tmp_next;
	}

	struct demand_node *new_node = (struct demand_node*) malloc(sizeof(struct demand_node));
	ps_list_init_d(new_node);
	new_node->abs_deadline = abs_deadline;
	new_node->demand = adjustment;
	new_node->tenant = sm->tenant;
	// new_node->sandbox_meta = sm;
	sm->demand_node = new_node;
	assert(ps_list_singleton_d(new_node));
	ps_list_append_d(node_spot, new_node);
	dbf->demand_total = demand_sum + adjustment;
	return true;
err:
	return false;
}

void
dbf_list_force_add_extra_slack(void *dbf_raw, struct sandbox_metadata *sm, uint64_t adjustment)
{
	assert(dbf_raw != NULL);
	assert(sm);
	assert(sm->demand_node);
	assert(adjustment > 0);

	struct demand_node *node = sm->demand_node;
	assert(node->abs_deadline == sm->absolute_deadline);
	assert(node->demand >= adjustment);
	node->demand += adjustment;

	struct dbf_list *dbf = (struct dbf_list *)dbf_raw;
	dbf->demand_total += adjustment;
}

void
dbf_list_reduce_demand(struct sandbox_metadata *sm, uint64_t adjustment, bool delete_node)
{
	assert(sm);
	assert(sm->demand_node);
	assert(delete_node || adjustment > 0);

	struct demand_node *node = sm->demand_node;
	assert(node->abs_deadline == sm->absolute_deadline);
	assert(node->demand >= adjustment);
	node->demand -= adjustment;

	// assert(dbf->demand_total >= adjustment);
	// dbf->demand_total -= adjustment;

	if (delete_node) {
		assert(node->demand == 0);
		/* Clean up empty and repetitive nodes */
		ps_list_rem_d(node);
		free(node);
		node = NULL;
	}
}

static void
dbf_list_free(void *dbf)
{
	assert(dbf != NULL);

	free(dbf);
}

void *
dbf_list_initialize(uint32_t num_of_workers, uint8_t reservation_percentile, int worker_idx, struct tenant *tenant)
{
	struct dbf_config config = {
		// .try_update_demand_fn = dbf_list_try_add_new_demand,
		.get_worker_idx_fn = dbf_list_get_worker_idx,
		.get_time_of_oversupply_fn = dbf_list_get_time_of_oversupply,
		.print_fn               = dbf_list_print,
		.free_fn = dbf_list_free
	};

	dbf_plug_functions(&config);

	assert(runtime_max_deadline > 0);
	
	struct dbf_list *dbf = (struct dbf_list *)calloc(1, sizeof(struct dbf_list));
	ps_list_head_init(&dbf->demands_list);

	dbf->max_relative_deadline = runtime_max_deadline;
	dbf->worker_idx            = worker_idx;
	// uint32_t cpu_factor        = (num_of_workers == 1) ? 1 : num_of_workers * RUNTIME_MAX_CPU_UTIL_PERCENTILE / 100;
	dbf->base_supply           = /*runtime_quantum * */1.0*num_of_workers * reservation_percentile * RUNTIME_MAX_CPU_UTIL_PERCENTILE / 10000;
	dbf->tenant = tenant;

	return dbf;
}
