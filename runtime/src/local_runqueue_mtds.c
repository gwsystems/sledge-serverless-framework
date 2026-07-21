#include <stdint.h>
#include <threads.h>

#include "arch/context.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "global_request_scheduler.h"
#include "local_runqueue.h"
#include "local_runqueue_mtds.h"
#include "panic.h"
#include "priority_queue.h"
#include "runtime.h"
#include "sandbox_functions.h"
#include "tenant_functions.h"

thread_local static struct priority_queue *local_runqueue_mtds_guaranteed;
thread_local static struct priority_queue *local_runqueue_mtds_default;

extern __thread struct priority_queue *worker_thread_timeout_queue;

/**
 * Get Per-Worker-Tenant priority for Priority Queue ordering
 * @param element the Worker_TW
 * @returns the priority of the head of the Worker_TW
 */
static inline uint64_t
perworker_tenant_get_priority(void *element)
{
	struct tenant_wrapper *worker_tw = (struct tenant_wrapper *)element;
	struct sandbox        *sandbox   = NULL;
	priority_queue_top_nolock(worker_tw->sandboxes, (void **)&sandbox);
	return (sandbox) ? sandbox->absolute_deadline : UINT64_MAX;
}

inline bool
worker_tw_is_in_guaranteed_queue(struct tenant_wrapper *worker_tw)
{
	assert(worker_tw);
	return worker_tw->destination_queue == local_runqueue_mtds_guaranteed;
}

/**
 * Checks if the run queue is empty
 * @returns true if empty. false otherwise
 */
bool
local_runqueue_mtds_is_empty()
{
	return priority_queue_length_nolock(local_runqueue_mtds_guaranteed) == 0
	       && priority_queue_length_nolock(local_runqueue_mtds_default) == 0;
}

/**
 * Adds a sandbox to the run queue
 * @param sandbox
 * @returns pointer to sandbox added
 */
void
local_runqueue_mtds_add(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	int rc = -1;

	struct tenant_wrapper *worker_tw = &sandbox->tenant->tw_wrappers[worker_thread_idx];

	/* Runs only once if the tenant queue is not in the runqueue */
	if (worker_tw->destination_queue == NULL) {
		worker_tw->destination_queue = local_runqueue_mtds_default;
		if (tenant_has_budget(sandbox->tenant)) worker_tw->destination_queue = local_runqueue_mtds_guaranteed;
	}

	struct priority_queue *destination_queue = worker_tw->destination_queue;

	/* Add the sandbox to the per-worker-tenant (worker_tw) queue */
	rc = priority_queue_enqueue_nolock(worker_tw->sandboxes, sandbox);
	assert(rc == 0);

	/* Add the tenant queue to the worker runqueue if the tenant is not there already */
	if (priority_queue_length_nolock(worker_tw->sandboxes) == 1) {
		rc = priority_queue_enqueue_nolock(destination_queue, worker_tw);
		assert(rc == 0);

		worker_tw->ds_timeout = worker_tw->tenant->defferable_server_timeout;
		rc                    = priority_queue_enqueue_nolock(worker_thread_timeout_queue, worker_tw);
		assert(rc == 0);

		return;
	}

	/* Maintain the minheap structure by deleting & adding the worker_tw */
	rc = priority_queue_delete_nolock(destination_queue, worker_tw);
	assert(rc == 0);
	rc = priority_queue_enqueue_nolock(destination_queue, worker_tw);
	assert(rc == 0);
}

/**
 * Deletes a sandbox from the runqueue
 * @param sandbox to delete
 */
static void
local_runqueue_mtds_delete(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	struct tenant_wrapper *worker_tw = &sandbox->tenant->tw_wrappers[worker_thread_idx];

	/* Delete the sandbox from the corresponding Per-Worker-Tenant queue */
	int rc = priority_queue_delete_nolock(worker_tw->sandboxes, sandbox);
	assert(rc == 0);

	struct priority_queue *destination_queue = worker_tw->destination_queue;

	/* Delete the Worker_TW from the local runqueue completely if worker_tw is empty, re-add otherwise to heapify */
	rc = priority_queue_delete_nolock(destination_queue, worker_tw);
	assert(rc == 0);

	/* Add the Worker_TW back to the local runqueue if it still has other sandboxes inside */
	if (priority_queue_length_nolock(worker_tw->sandboxes) > 0) {
		rc = priority_queue_enqueue_nolock(destination_queue, worker_tw);
		assert(rc == 0);
	} else {
		rc = priority_queue_delete_nolock(worker_thread_timeout_queue, worker_tw);
		assert(rc == 0);
		worker_tw->destination_queue = NULL;
	}
}

/**
 * This function determines the next sandbox to run.
 * This is the head of the runqueue
 *
 * Execute the sandbox at the head of the thread local runqueue
 * @return the sandbox to execute or NULL if none are available
 */
struct sandbox *
local_runqueue_mtds_get_next()
{
	/* Get the deadline of the sandbox at the head of the local request queue */
	struct tenant_wrapper *next_tw = NULL;
	struct priority_queue *dq      = local_runqueue_mtds_guaranteed;

	/* Check the local guaranteed queue for any potential demotions */
	int rc = priority_queue_top_nolock(dq, (void **)&next_tw);
	while (rc != -ENOENT && next_tw->tenant->remaining_budget <= 0) {
		local_runqueue_mtds_demote(next_tw);
		rc = priority_queue_top_nolock(dq, (void **)&next_tw);
	}

	if (rc == -ENOENT) {
		dq = local_runqueue_mtds_default;
		rc = priority_queue_top_nolock(dq, (void **)&next_tw);
		if (rc == -ENOENT) return NULL;
		assert(next_tw->destination_queue == local_runqueue_mtds_default);
	} else {
		assert(next_tw->destination_queue == local_runqueue_mtds_guaranteed);
	}

	struct sandbox *next_sandbox = NULL;
	priority_queue_top_nolock(next_tw->sandboxes, (void **)&next_sandbox);
	assert(next_sandbox);

	return next_sandbox;
}

/**
 * Registers the PS variant with the polymorphic interface
 */
void
local_runqueue_mtds_initialize()
{
	/* Initialize local state */
	local_runqueue_mtds_guaranteed = priority_queue_initialize(RUNTIME_MAX_TENANT_COUNT, false,
	                                                           perworker_tenant_get_priority);
	local_runqueue_mtds_default    = priority_queue_initialize(RUNTIME_MAX_TENANT_COUNT, false,
	                                                           perworker_tenant_get_priority);

	/* Register Function Pointers for Abstract Scheduling API */
	struct local_runqueue_config config = {.add_fn      = local_runqueue_mtds_add,
	                                       .is_empty_fn = local_runqueue_mtds_is_empty,
	                                       .delete_fn   = local_runqueue_mtds_delete,
	                                       .get_next_fn = local_runqueue_mtds_get_next};

	local_runqueue_initialize(&config);
}

/**
 * Promotes the given per-worker-tenant queue, which means deletes from the Default queue
 *  and adds to the Guaranteed queue.
 */
void
local_runqueue_mtds_promote(struct tenant_wrapper *worker_tw)
{
	assert(worker_tw != NULL);

	if (worker_tw->destination_queue == local_runqueue_mtds_guaranteed) return;

	/* Delete the corresponding Worker_TW from the Default queue */
	int rc = priority_queue_delete_nolock(local_runqueue_mtds_default, worker_tw);
	assert(rc == 0);
	/* Add the corresponding Worker_TW to the Guaranteed queue */
	rc = priority_queue_enqueue_nolock(local_runqueue_mtds_guaranteed, worker_tw);
	assert(rc == 0);

	worker_tw->destination_queue = local_runqueue_mtds_guaranteed;
	// debuglog("Promoted '%s' locally", worker_tw->tenant->name);
}

/**
 * Demotes the given per-worker-tenant queue: Delete from the Guaranteed queue, add to the Default queue.
 */
void
local_runqueue_mtds_demote(struct tenant_wrapper *worker_tw)
{
	assert(worker_tw != NULL);
	assert(worker_tw->destination_queue = local_runqueue_mtds_guaranteed);

	/* Move the corresponding Worker_TW from the Guaranteed queue to the Default queue */
	int rc = priority_queue_delete_nolock(local_runqueue_mtds_guaranteed, worker_tw);
	assert(rc == 0);
	rc = priority_queue_enqueue_nolock(local_runqueue_mtds_default, worker_tw);
	assert(rc == 0);

	worker_tw->destination_queue = local_runqueue_mtds_default;
	// debuglog("Demoted '%s' locally", worker_tw->tenant->name);
}


/*
 * Checks if there are any tenant timers that run out in the LOCAL queue,
 *  if so promote that tenant.
 */
void
local_timeout_queue_process_promotions()
{
	struct tenant_wrapper *worker_tw = NULL;

	/* Check the timeout queue for a potential tenant to get Promoted */
	priority_queue_top_nolock(worker_thread_timeout_queue, (void **)&worker_tw);
	assert(worker_tw);

	const uint64_t now = __getcycles();

	while (now >= worker_tw->ds_timeout) {
		assert(priority_queue_length_nolock(worker_tw->sandboxes) > 0);

		if (tenant_has_budget(worker_tw->tenant) > 0) { local_runqueue_mtds_promote(worker_tw); }

		/* Reheapify the timeout queue with the updated timeout value of the tenant */
		priority_queue_delete_nolock(worker_thread_timeout_queue, worker_tw);
		worker_tw->ds_timeout = worker_tw->tenant->defferable_server_timeout;
		priority_queue_enqueue_nolock(worker_thread_timeout_queue, worker_tw);

		priority_queue_top_nolock(worker_thread_timeout_queue, (void **)&worker_tw);
	}
}
