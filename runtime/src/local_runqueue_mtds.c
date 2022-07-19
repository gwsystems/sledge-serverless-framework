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
#include "sandbox_functions.h"
#include "tenant_functions.h"
#include "runtime.h"

thread_local static struct priority_queue *local_runqueue_mtds_guaranteed;
thread_local static struct priority_queue *local_runqueue_mtds_default;

extern __thread struct priority_queue *worker_thread_timeout_queue;

/**
 * Get Per-Worker-Tenant priority for Priority Queue ordering
 * @param element the PWT
 * @returns the priority of the head of the PWT
 */
static inline uint64_t
perworker_tenant_get_priority(void *element)
{
	struct perworker_tenant_sandbox_queue *pwt     = (struct perworker_tenant_sandbox_queue *)element;
	struct sandbox                        *sandbox = NULL;
	priority_queue_top_nolock(pwt->sandboxes, (void **)&sandbox);
	return (sandbox) ? sandbox->absolute_deadline : UINT64_MAX;
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

	struct perworker_tenant_sandbox_queue *pwt = &sandbox->tenant->pwt_sandboxes[worker_thread_idx];
	struct priority_queue *destination_queue   = pwt->mt_class == MT_GUARANTEED ? local_runqueue_mtds_guaranteed
	                                                                            : local_runqueue_mtds_default;

	uint64_t prev_pwt_deadline = priority_queue_peek(pwt->sandboxes);

	/* Add the sandbox to the per-worker-tenant (pwt) queue */
	int rc = priority_queue_enqueue_nolock(pwt->sandboxes, sandbox);
	if (rc == -ENOSPC) panic("Per Worker Tenant queue is full!\n");
	// debuglog("Added a sandbox to the PWT of tenant '%s'", sandbox->tenant->name);

	/* If the tenant was not in the local runqueue, then since we are the first sandbox for it in this worker, add
	 * the tenant.  */
	if (priority_queue_length_nolock(pwt->sandboxes) == 1) {
		/* Add sandbox tenant to the worker's timeout queue if guaranteed tenant and only if first sandbox*/
		if (tenant_is_paid(sandbox->tenant)) {
			pwt->tenant_timeout.timeout = get_next_timeout_of_tenant(sandbox->tenant->replenishment_period);
			priority_queue_enqueue_nolock(worker_thread_timeout_queue, &pwt->tenant_timeout);
		}

		rc = priority_queue_enqueue_nolock(destination_queue, pwt);

		return;
	}

	if (sandbox->absolute_deadline < prev_pwt_deadline) {
		/* Maintain the minheap structure by deleting & adding the pwt.
		 * Do this only when the pwt's priority is updated. */
		rc = priority_queue_delete_nolock(destination_queue, pwt);
		assert(rc == 0);
		rc = priority_queue_enqueue_nolock(destination_queue, pwt);
		if (rc == -ENOSPC) panic("Worker Local Runqueue is full!\n");
	}
}

/**
 * Deletes a sandbox from the runqueue
 * @param sandbox to delete
 */
static void
local_runqueue_mtds_delete(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	struct perworker_tenant_sandbox_queue *pwt = &sandbox->tenant->pwt_sandboxes[worker_thread_idx];

	/* Delete the sandbox from the corresponding Per-Worker-Tenant queue */
	if (priority_queue_delete_nolock(pwt->sandboxes, sandbox) == -1) {
		panic("Tried to delete sandbox %lu from PWT queue, but was not present\n", sandbox->id);
	}

	struct priority_queue *destination_queue = local_runqueue_mtds_default;

	if (pwt->mt_class == MT_GUARANTEED) { destination_queue = local_runqueue_mtds_guaranteed; }


	/* Delete the PWT from the local runqueue completely if pwt is empty, re-add otherwise to heapify */
	if (priority_queue_delete_nolock(destination_queue, pwt) == -1) {
		// TODO: Apply the composite way of PQ deletion O(logn)
		panic("Tried to delete a PWT of %s from local runqueue, but was not present\n", pwt->tenant->name);
	}

	/* Add the PWT back to the local runqueue if it still has other sandboxes inside */
	if (priority_queue_length_nolock(pwt->sandboxes) > 0) {
		priority_queue_enqueue_nolock(destination_queue, pwt);
	} else if (tenant_is_paid(pwt->tenant)) {
		priority_queue_delete_nolock(worker_thread_timeout_queue, &pwt->tenant_timeout);
		pwt->mt_class = MT_GUARANTEED;
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
	struct perworker_tenant_sandbox_queue *next_pwt = NULL;
	struct priority_queue                 *dq       = local_runqueue_mtds_guaranteed;

	/* Check the local guaranteed queue for any potential demotions */
	int rc = priority_queue_top_nolock(dq, (void **)&next_pwt);
	while (rc != -ENOENT && next_pwt->tenant->remaining_budget <= 0) { // next_pwt->mt_class==MT_DEFAULT){
		local_runqueue_mtds_demote(next_pwt);
		// debuglog("Demoted '%s' locally in GetNext", next_pwt->tenant->name);
		next_pwt->mt_class = MT_DEFAULT;
		rc                 = priority_queue_top_nolock(dq, (void **)&next_pwt);
	}

	if (rc == -ENOENT) {
		dq = local_runqueue_mtds_default;
		rc = priority_queue_top_nolock(dq, (void **)&next_pwt);
		if (rc == -ENOENT) return NULL;
	}

	struct sandbox *next_sandbox = NULL;
	priority_queue_top_nolock(next_pwt->sandboxes, (void **)&next_sandbox);
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
	struct local_runqueue_config config = { .add_fn      = local_runqueue_mtds_add,
		                                .is_empty_fn = local_runqueue_mtds_is_empty,
		                                .delete_fn   = local_runqueue_mtds_delete,
		                                .get_next_fn = local_runqueue_mtds_get_next };

	local_runqueue_initialize(&config);
}

/**
 * Promotes the given per-worker-tenant queue, which means deletes from the Default queue
 *  and adds to the Guaranteed queue.
 */
void
local_runqueue_mtds_promote(struct perworker_tenant_sandbox_queue *pwt)
{
	assert(pwt != NULL);

	/* Delete the corresponding PWT from the Guaranteed queue */
	int rc = priority_queue_delete_nolock(local_runqueue_mtds_default, pwt);
	if (rc == -1) {
		panic("Tried to delete a non-present PWT of %s from the Local Default queue. Already deleted? , ITS "
		      "SIZE: %d",
		      pwt->tenant->name, priority_queue_length_nolock(pwt->sandboxes));
	}

	/* Add the corresponding PWT to the Default queue */
	rc = priority_queue_enqueue_nolock(local_runqueue_mtds_guaranteed, pwt);
	if (rc == -ENOSPC) panic("Local Guaranteed queue is full!\n");
}

/**
 * Demotes the given per-worker-tenant queue, which means deletes from the Guaranteed queue
 *  and adds to the Default queue.
 */
void
local_runqueue_mtds_demote(struct perworker_tenant_sandbox_queue *pwt)
{
	assert(pwt != NULL);

	/* Delete the corresponding PWT from the Guaranteed queue */
	int rc = priority_queue_delete_nolock(local_runqueue_mtds_guaranteed, pwt);
	if (rc == -1) {
		panic("Tried to delete a non-present PWT of %s from the Local Guaranteed queue. Already deleted? , ITS "
		      "SIZE: %d",
		      pwt->tenant->name, priority_queue_length_nolock(pwt->sandboxes));
	}

	/* Add the corresponding PWT to the Default queue */
	rc = priority_queue_enqueue_nolock(local_runqueue_mtds_default, pwt);
	if (rc == -ENOSPC) panic("Local Default queue is full!\n");
}


/*
 * Checks if there are any tenant timers that run out in the LOCAL queue,
 *  if so promote that tenant.
 */
void
local_timeout_queue_process_promotions()
{
	struct tenant_timeout *top_tenant_timeout = NULL;

	/* Check the timeout queue for a potential tenant to get PRomoted */
	priority_queue_top_nolock(worker_thread_timeout_queue, (void **)&top_tenant_timeout);
	if (top_tenant_timeout == NULL) return; // no guaranteed tenants

	struct perworker_tenant_sandbox_queue *pwt_to_promote = NULL;
	uint64_t                               now            = __getcycles();

	while (now >= top_tenant_timeout->timeout) {
		pwt_to_promote = top_tenant_timeout->pwt;
		assert(priority_queue_length_nolock(pwt_to_promote->sandboxes) > 0);

		if (pwt_to_promote->mt_class == MT_DEFAULT) {
			local_runqueue_mtds_promote(pwt_to_promote);
			pwt_to_promote->mt_class = MT_GUARANTEED;
			// debuglog("Promoted '%s' locally", top_tenant_timeout->tenant->name);
		}

		/* Reheapify the timeout queue with the updated timeout value of the tenant */
		priority_queue_delete_nolock(worker_thread_timeout_queue, top_tenant_timeout);
		top_tenant_timeout->timeout = get_next_timeout_of_tenant(
		  top_tenant_timeout->tenant->replenishment_period);
		priority_queue_enqueue_nolock(worker_thread_timeout_queue, top_tenant_timeout);

		priority_queue_top_nolock(worker_thread_timeout_queue, (void **)&top_tenant_timeout);
	}
}
