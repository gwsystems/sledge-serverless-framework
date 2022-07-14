#include <assert.h>
#include <errno.h>

#include "global_request_scheduler.h"
#include "listener_thread.h"
#include "panic.h"
#include "priority_queue.h"
#include "runtime.h"
#include "tenant_functions.h"

static struct priority_queue *global_request_scheduler_mtds_guaranteed;
static struct priority_queue *global_request_scheduler_mtds_default;
static struct priority_queue *global_tenant_timeout_queue;
static lock_t                 global_lock;

static inline uint64_t
tenant_request_queue_get_priority(void *element)
{
	struct tenant_global_request_queue *tgrq    = (struct tenant_global_request_queue *)element;
	struct sandbox                     *sandbox = NULL;
	priority_queue_top_nolock(tgrq->sandbox_requests, (void **)&sandbox);
	return (sandbox) ? sandbox->absolute_deadline : UINT64_MAX;
}

/**
 * Demotes the given tenant's request queue, which means deletes the TGRQ from the Guaranteed queue
 *  and adds to the Default queue.
 */
void
global_request_scheduler_mtds_demote_nolock(struct tenant_global_request_queue *tgrq)
{
	assert(tgrq != NULL);

	if (tgrq->mt_class == MT_DEFAULT) return;

	/* Delete the corresponding TGRQ from the Guaranteed queue */
	int rc = priority_queue_delete_nolock(global_request_scheduler_mtds_guaranteed, tgrq);
	if (rc == -1) {
		panic("Tried to delete a non-present TGRQ from the Global Guaranteed queue. Already deleted?, ITS "
		      "SIZE: %d",
		      priority_queue_length_nolock(tgrq->sandbox_requests));
	}

	/* Add the corresponding TGRQ to the Default queue */
	rc = priority_queue_enqueue_nolock(global_request_scheduler_mtds_default, tgrq);
	if (rc == -ENOSPC) panic("Global Default queue is full!\n");
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
	if (unlikely(!listener_thread_is_running())) panic("%s is only callable by the listener thread\n", __func__);

	struct tenant_global_request_queue *tgrq = sandbox->tenant->tgrq_requests;

	LOCK_LOCK(&global_lock);

	struct priority_queue *destination_queue = global_request_scheduler_mtds_default;
	if (sandbox->tenant->tgrq_requests->mt_class == MT_GUARANTEED) {
		destination_queue = global_request_scheduler_mtds_guaranteed;
	}

	uint64_t last_mrq_deadline = priority_queue_peek(tgrq->sandbox_requests);

	int rc = priority_queue_enqueue_nolock(tgrq->sandbox_requests, sandbox);
	if (rc == -ENOSPC) panic("Tenant's Request Queue is full\n");
	// debuglog("Added a sandbox to the TGRQ");

	/* Maintain the minheap structure by Removing and Adding the TGRQ from and to the global runqueue.
	 * Do this only when the TGRQ's priority is updated.
	 */
	if (priority_queue_peek(tgrq->sandbox_requests) < last_mrq_deadline) {
		priority_queue_delete_nolock(destination_queue, tgrq);

		rc = priority_queue_enqueue_nolock(destination_queue, tgrq);
		if (rc == -ENOSPC) panic("Global Runqueue is full!\n");
		// debuglog("Added the TGRQ back to the Global runqueue - %s to Heapify", QUEUE_NAME);
	}

	LOCK_UNLOCK(&global_lock);

	return sandbox;
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
	/* This function won't be used with the MTDS scheduler. Keeping merely for the polymorhism. */
	return -1;
}

/**
 * @param removed_sandbox pointer to set to removed sandbox request
 * @param target_deadline the deadline that the request must be earlier than to dequeue
 * @param mt_class the multi-tenancy class of the global request to compare the target deadline against
 * @returns 0 if successful, -ENOENT if empty or if request isn't earlier than target_deadline
 */
int
global_request_scheduler_mtds_remove_with_mt_class(struct sandbox **removed_sandbox, uint64_t target_deadline,
                                                   enum MULTI_TENANCY_CLASS target_mt_class)
{
	int rc = -ENOENT;
	;

	LOCK_LOCK(&global_lock);

	/* Avoid unnessary locks when the target_deadline is tighter than the head of the Global runqueue */
	uint64_t global_guaranteed_deadline = priority_queue_peek(global_request_scheduler_mtds_guaranteed);
	uint64_t global_default_deadline    = priority_queue_peek(global_request_scheduler_mtds_default);

	switch (target_mt_class) {
	case MT_GUARANTEED:
		if (global_guaranteed_deadline >= target_deadline) goto done;
		break;
	case MT_DEFAULT:
		if (global_guaranteed_deadline == UINT64_MAX && global_default_deadline >= target_deadline) goto done;
		break;
	}

	struct tenant_global_request_queue *top_tgrq          = NULL;
	struct priority_queue              *destination_queue = global_request_scheduler_mtds_guaranteed;

	/* Spot the Tenant Global Request Queue (TGRQ) to remove the sandbox request from */
	rc = priority_queue_top_nolock(destination_queue, (void **)&top_tgrq);
	if (rc == -ENOENT) {
		if (target_mt_class == MT_GUARANTEED) goto done;

		destination_queue = global_request_scheduler_mtds_default;

		rc = priority_queue_top_nolock(destination_queue, (void **)&top_tgrq);
		if (rc == -ENOENT) goto done;
	} else {
		if (top_tgrq->mt_class == MT_GUARANTEED && top_tgrq->tenant->remaining_budget <= 0) {
			global_request_scheduler_mtds_demote_nolock(top_tgrq);
			// debuglog("Demoted '%s' GLOBALLY", top_tgrq->tenant->name);
			top_tgrq->mt_class = MT_DEFAULT;

			rc = -ENOENT;
			goto done;
		}
	}

	assert(top_tgrq);

	/* Remove the sandbox from the corresponding TGRQ */
	rc = priority_queue_dequeue_nolock(top_tgrq->sandbox_requests, (void **)removed_sandbox);
	assert(rc == 0);

	/* Delete the TGRQ from the global runqueue completely if TGRQ is empty, re-add otherwise to heapify */
	if (priority_queue_delete_nolock(destination_queue, top_tgrq) == -1) {
		panic("Tried to delete an TGRQ from the Global runqueue, but was not present");
	}

	if (priority_queue_length_nolock(top_tgrq->sandbox_requests) > 0) {
		priority_queue_enqueue_nolock(destination_queue, top_tgrq);
	}

done:
	LOCK_UNLOCK(&global_lock);
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


/**
 * Initializes the variant and registers against the polymorphic interface
 */
void
global_request_scheduler_mtds_initialize()
{
	global_request_scheduler_mtds_guaranteed = priority_queue_initialize(RUNTIME_RUNQUEUE_SIZE, false,
	                                                                     tenant_request_queue_get_priority);
	global_request_scheduler_mtds_default    = priority_queue_initialize(RUNTIME_RUNQUEUE_SIZE, false,
	                                                                     tenant_request_queue_get_priority);

	global_tenant_timeout_queue = priority_queue_initialize(RUNTIME_RUNQUEUE_SIZE, false,
	                                                        tenant_timeout_get_priority);

	LOCK_INIT(&global_lock);

	struct global_request_scheduler_config config = {
		.add_fn                  = global_request_scheduler_mtds_add,
		.remove_fn               = global_request_scheduler_mtds_remove,
		.remove_if_earlier_fn    = global_request_scheduler_mtds_remove_if_earlier,
		.remove_with_mt_class_fn = global_request_scheduler_mtds_remove_with_mt_class,
		.peek_fn                 = global_request_scheduler_mtds_peek
	};

	global_request_scheduler_initialize(&config);
}

void
global_request_scheduler_mtds_free()
{
	priority_queue_free(global_request_scheduler_mtds_guaranteed);
	priority_queue_free(global_request_scheduler_mtds_default);
	priority_queue_free(global_tenant_timeout_queue);
}


void
global_timeout_queue_add(struct tenant *tenant)
{
	tenant->tgrq_requests->tenant_timeout.timeout = get_next_timeout_of_tenant(tenant->replenishment_period);
	priority_queue_enqueue_nolock(global_tenant_timeout_queue, &tenant->tgrq_requests->tenant_timeout);
}

/**
 * Promotes the given tenant, which means deletes from the Default queue
 *  and adds to the Guaranteed queue.
 */
void
global_request_scheduler_mtds_promote_lock(struct tenant_global_request_queue *tgrq)
{
	assert(tgrq != NULL);
	// assert(priority_queue_length_nolock(tgrq->sandbox_requests) == 0);

	LOCK_LOCK(&global_lock);

	if (tgrq->mt_class == MT_GUARANTEED) goto done;
	if (priority_queue_length_nolock(tgrq->sandbox_requests) == 0) goto done;

	/* Delete the corresponding TGRQ from the Guaranteed queue */
	int rc = priority_queue_delete_nolock(global_request_scheduler_mtds_default, tgrq);
	if (rc == -1) {
		panic("Tried to delete a non-present TGRQ from the Global Default queue. Already deleted?, ITS SIZE: "
		      "%d",
		      priority_queue_length_nolock(tgrq->sandbox_requests));
	}

	/* Add the corresponding TGRQ to the Default queue */
	rc = priority_queue_enqueue_nolock(global_request_scheduler_mtds_guaranteed, tgrq);
	if (rc == -ENOSPC) panic("Global Guaranteed queue is full!\n");

done:
	LOCK_UNLOCK(&global_lock);
}

/*
 * Checks if there are any tenant timers that run out in the GLOBAL queue,
 *  if so promote that tenant.
 */
void
global_timeout_queue_process_promotions()
{
	struct tenant_timeout *top_tenant_timeout = NULL;

	/* Check the timeout queue for a potential tenant to get PRomoted */
	priority_queue_top_nolock(global_tenant_timeout_queue, (void **)&top_tenant_timeout);
	if (top_tenant_timeout == NULL) return; // no guaranteed tenants

	struct tenant	              *tenant          = NULL;
	struct tenant_global_request_queue *tgrq_to_promote = NULL;
	uint64_t                            now             = __getcycles();
	int64_t                             prev_budget;

	while (now >= top_tenant_timeout->timeout) {
		tenant          = top_tenant_timeout->tenant;
		tgrq_to_promote = tenant->tgrq_requests;

		if (tgrq_to_promote->mt_class == MT_DEFAULT) {
			if (priority_queue_length_nolock(tgrq_to_promote->sandbox_requests) > 0)
				global_request_scheduler_mtds_promote_lock(tgrq_to_promote);
			tgrq_to_promote->mt_class = MT_GUARANTEED;
			// debuglog("Promoted '%s' GLOBALLY", tenant->name);
		}

		// TODO: We need a smarter technique to reset budget to consider budget overusage:
		prev_budget = atomic_load(&tenant->remaining_budget);
		while (!atomic_compare_exchange_strong(&tenant->remaining_budget, &prev_budget, tenant->max_budget))
			;

		/* Reheapify the timeout queue with the updated timeout value of the tenant */
		priority_queue_delete_nolock(global_tenant_timeout_queue, top_tenant_timeout);
		top_tenant_timeout->timeout = get_next_timeout_of_tenant(tenant->replenishment_period);
		priority_queue_enqueue_nolock(global_tenant_timeout_queue, top_tenant_timeout);

		priority_queue_top_nolock(global_tenant_timeout_queue, (void **)&top_tenant_timeout);
		now = __getcycles();
	}
}
