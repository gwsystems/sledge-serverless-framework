#include <assert.h>
#include <errno.h>

#include "global_request_scheduler.h"
#include "listener_thread.h"
#include "local_runqueue_mtds.h"
#include "panic.h"
#include "priority_queue.h"
#include "runtime.h"
#include "sandbox_set_as_error.h"
#include "tenant_functions.h"

static struct priority_queue *global_request_scheduler_mtds_guaranteed;
static struct priority_queue *global_request_scheduler_mtds_default;
static struct priority_queue *global_tenant_timeout_queue;
static lock_t                 global_lock;

static inline uint64_t
tenant_request_queue_get_priority(void *element)
{
	struct tenant_wrapper *global_tw = (struct tenant_wrapper *)element;
	struct sandbox        *sandbox   = NULL;
	priority_queue_top_nolock(global_tw->sandboxes, (void **)&sandbox);
	return (sandbox) ? sandbox->absolute_deadline : UINT64_MAX;
}

/**
 * Demotes the given tenant's request queue, which means deletes the Global_TW from the Guaranteed queue
 *  and adds to the Default queue.
 */
void
global_request_scheduler_mtds_demote_nolock(struct tenant_wrapper *global_tw)
{
	assert(global_tw != NULL);
	assert(global_tw->destination_queue == global_request_scheduler_mtds_guaranteed);

	/* Delete the corresponding Global_TW from the Guaranteed queue */
	int rc = priority_queue_delete_nolock(global_request_scheduler_mtds_guaranteed, global_tw);
	assert(rc == 0);
	/* Add the corresponding Global_TW to the Default queue */
	rc = priority_queue_enqueue_nolock(global_request_scheduler_mtds_default, global_tw);
	assert(rc == 0);

	global_tw->destination_queue = global_request_scheduler_mtds_default;
	// debuglog("Demoted '%s' GLOBALLY", global_tw->tenant->name);
}

/**
 * Pushes a sandbox request to the global runqueue
 * @param sandbox
 * @returns pointer to request if added. NULL otherwise
 */
static struct sandbox *
global_request_scheduler_mtds_add(struct sandbox *sandbox)
{
	assert(sandbox);
	assert(global_request_scheduler_mtds_guaranteed && global_request_scheduler_mtds_default);
	assert(listener_thread_is_running());

	int rc = -1;

	struct tenant_wrapper *global_tw = sandbox->tenant->global_tenant_wrapper;
	/* Runs only once at the very first request in the global runqueue */
	if (unlikely(global_tw->destination_queue == NULL)) {
		global_tw->destination_queue = global_request_scheduler_mtds_default;
		if (tenant_has_budget(sandbox->tenant)) {
			global_tw->destination_queue = global_request_scheduler_mtds_guaranteed;
		}
		rc = priority_queue_enqueue_nolock(global_tw->destination_queue, global_tw);
		assert(rc == 0);

		struct tenant *tenant = sandbox->tenant;

		tenant->defferable_server_timeout = __getcycles() + tenant->replenishment_period;
		priority_queue_enqueue_nolock(global_tenant_timeout_queue, tenant);
	}

	lock_node_t node = {};
	lock_lock(&global_lock, &node);

	struct priority_queue *destination_queue = global_tw->destination_queue;

	rc = priority_queue_enqueue_nolock(global_tw->sandboxes, sandbox);
	if (rc == -ENOSPC) goto err;

	/* Maintain the minheap structure by Removing and Re-adding the Global_TW */
	rc = priority_queue_delete_nolock(destination_queue, global_tw);
	assert(rc == 0);
	rc = priority_queue_enqueue_nolock(destination_queue, global_tw);
	assert(rc == 0);

done:
	lock_unlock(&global_lock, &node);
	return sandbox;

err:
	sandbox = NULL;
	goto done;
}

/**
 * @param pointer to the pointer that we want to set to the address of the removed sandbox request
 * @returns 0 if successful, -ENOENT if empty
 */
int
global_request_scheduler_mtds_remove(struct sandbox **removed_sandbox)
{
	/* This function won't be used with the MTDS scheduler. Keeping merely for the polymorhism. */
	return -1;
}


/**
 * @param removed_sandbox pointer to set to removed sandbox request
 * @param target_deadline the deadline that the request must be earlier than to dequeue
 * @returns 0 if successful, -ENOENT if empty or if request isn't earlier than target_deadline
 */
int
global_request_scheduler_mtds_remove_if_earlier(struct sandbox **removed_sandbox, uint64_t target_deadline)
{
	int                    rc                                   = -ENOENT;
	struct sandbox        *local                                = local_runqueue_get_next();
	struct tenant_wrapper *worker_tw                            = NULL;
	bool                   local_sandbox_is_in_guaranteed_queue = false;

	if (local) {
		worker_tw = &local->tenant->tw_wrappers[worker_thread_idx];
		if (worker_tw_is_in_guaranteed_queue(worker_tw)) local_sandbox_is_in_guaranteed_queue = true;
	}

	lock_node_t node = {};
	lock_lock(&global_lock, &node);

	uint64_t global_guaranteed_deadline = priority_queue_peek(global_request_scheduler_mtds_guaranteed);
	uint64_t global_default_deadline    = priority_queue_peek(global_request_scheduler_mtds_default);

	if (local_sandbox_is_in_guaranteed_queue) {
		if (global_guaranteed_deadline >= target_deadline) goto done;
	} else {
		if (global_guaranteed_deadline == UINT64_MAX && global_default_deadline >= target_deadline) goto done;
	}

	struct tenant_wrapper *top_global_tw     = NULL;
	struct priority_queue *destination_queue = global_request_scheduler_mtds_guaranteed;

	/* Spot the Tenant Global Request Queue (Global_TW) to remove the sandbox request from */
	priority_queue_top_nolock(destination_queue, (void **)&top_global_tw);
	if (global_guaranteed_deadline == UINT64_MAX) {
		/* No guaranteed tenant request in the global runqueue */
		assert(!local_sandbox_is_in_guaranteed_queue);

		destination_queue = global_request_scheduler_mtds_default;

		rc = priority_queue_top_nolock(destination_queue, (void **)&top_global_tw);
		// if (rc == -ENOENT) goto done;
		assert(rc == 0); // We are not removing global_tws from the runqueue, even if empty
	} else {
		assert(top_global_tw->destination_queue == global_request_scheduler_mtds_guaranteed);
		if (top_global_tw->tenant->remaining_budget <= 0) {
			global_request_scheduler_mtds_demote_nolock(top_global_tw);

			rc = -ENOENT;
			goto done;
		}
	}

	assert(top_global_tw);

	/* Remove the sandbox from the corresponding Global_TW */
	rc = priority_queue_dequeue_nolock(top_global_tw->sandboxes, (void **)removed_sandbox);
	assert(rc == 0);

	/* Delete the Global_TW from the global runqueue and then re-add to heapify */
	rc = priority_queue_delete_nolock(destination_queue, top_global_tw);
	assert(rc == 0);
	rc = priority_queue_enqueue_nolock(destination_queue, top_global_tw);
	assert(rc == 0);

done:
	lock_unlock(&global_lock, &node);
	return rc;
}

/**
 * Peek at the priority of the highest priority task without having to take the lock
 * Because this is a min-heap PQ, the highest priority is the lowest 64-bit integer
 * This is used to store an absolute deadline
 * @returns value of highest priority value in queue or ULONG_MAX if empty
 */
static uint64_t
global_request_scheduler_mtds_peek(void)
{
	uint64_t val = priority_queue_peek(global_request_scheduler_mtds_guaranteed);
	if (val == UINT64_MAX) val = priority_queue_peek(global_request_scheduler_mtds_default);

	return val;
}


uint64_t
global_request_scheduler_mtds_guaranteed_peek(void)
{
	return priority_queue_peek(global_request_scheduler_mtds_guaranteed);
}

uint64_t
global_request_scheduler_mtds_default_peek(void)
{
	return priority_queue_peek(global_request_scheduler_mtds_default);
}

static inline uint64_t
tenant_timeout_get_priority(void *element)
{
	return ((struct tenant *)element)->defferable_server_timeout;
}

/**
 * Initializes the variant and registers against the polymorphic interface
 */
void
global_request_scheduler_mtds_initialize()
{
	global_request_scheduler_mtds_guaranteed = priority_queue_initialize(RUNTIME_MAX_TENANT_COUNT, false,
	                                                                     tenant_request_queue_get_priority);
	global_request_scheduler_mtds_default    = priority_queue_initialize(RUNTIME_MAX_TENANT_COUNT, false,
	                                                                     tenant_request_queue_get_priority);

	global_tenant_timeout_queue = priority_queue_initialize(RUNTIME_MAX_TENANT_COUNT, false,
	                                                        tenant_timeout_get_priority);

	lock_init(&global_lock);

	struct global_request_scheduler_config config = {.add_fn    = global_request_scheduler_mtds_add,
	                                                 .remove_fn = global_request_scheduler_mtds_remove,
	                                                 .remove_if_earlier_fn =
	                                                   global_request_scheduler_mtds_remove_if_earlier,
	                                                 .peek_fn = global_request_scheduler_mtds_peek};

	global_request_scheduler_initialize(&config);
}

void
global_request_scheduler_mtds_free()
{
	priority_queue_free(global_request_scheduler_mtds_guaranteed);
	priority_queue_free(global_request_scheduler_mtds_default);
	priority_queue_free(global_tenant_timeout_queue);
}


/**
 * Promotes the given tenant, which means deletes from the Default queue
 *  and adds to the Guaranteed queue.
 */
void
global_request_scheduler_mtds_promote_lock(struct tenant_wrapper *global_tw)
{
	assert(global_tw != NULL);
	assert(listener_thread_is_running());

	lock_node_t node = {};
	lock_lock(&global_lock, &node);

	if (global_tw->destination_queue == global_request_scheduler_mtds_guaranteed) goto done;

	/* Delete the corresponding Global_TW from the Guaranteed queue */
	int rc = priority_queue_delete_nolock(global_request_scheduler_mtds_default, global_tw);
	assert(rc == 0);
	/* Add the corresponding Global_TW to the Default queue */
	rc = priority_queue_enqueue_nolock(global_request_scheduler_mtds_guaranteed, global_tw);
	assert(rc == 0);

	global_tw->destination_queue = global_request_scheduler_mtds_guaranteed;

	// debuglog("Promoted '%s' GLOBALLY", global_tw->tenant->name);

done:
	lock_unlock(&global_lock, &node);
}

/*
 * Checks if there are any tenant timers that run out in the GLOBAL queue,
 *  if so promote that tenant.
 */
void
global_timeout_queue_process_promotions()
{
	assert(listener_thread_is_running());

	struct tenant         *tenant    = NULL;
	struct tenant_wrapper *global_tw = NULL;
	const uint64_t         now       = __getcycles();
	int64_t                prev_budget;

	/* Check the timeout queue for a potential tenant to get Promoted */
	priority_queue_top_nolock(global_tenant_timeout_queue, (void **)&tenant);
	if (unlikely(tenant == NULL)) return; /* no tenants yet */

	while (now >= tenant->defferable_server_timeout) {
		global_tw = tenant->global_tenant_wrapper;

		if (global_tw->destination_queue == global_request_scheduler_mtds_default) {
			global_request_scheduler_mtds_promote_lock(global_tw);
		}

		// TODO: We need a smarter technique to reset budget to consider budget overusage:
		prev_budget = atomic_load(&tenant->remaining_budget);
		while (!atomic_compare_exchange_strong(&tenant->remaining_budget, &prev_budget, tenant->max_budget));

		/* Reheapify the timeout queue with the updated timeout value of the tenant */
		int rc = priority_queue_delete_nolock(global_tenant_timeout_queue, tenant);
		assert(rc == 0);

		tenant->defferable_server_timeout = now + tenant->replenishment_period;

		rc = priority_queue_enqueue_nolock(global_tenant_timeout_queue, tenant);
		assert(rc == 0);

		rc = priority_queue_top_nolock(global_tenant_timeout_queue, (void **)&tenant);
		assert(rc == 0);
	}
}
