#include <assert.h>
#include <errno.h>

#include "global_request_scheduler.h"
#include "listener_thread.h"
#include "panic.h"
#include "priority_queue.h"
#include "runtime.h"

static struct priority_queue *global_request_scheduler_mts_guaranteed;
static struct priority_queue *global_request_scheduler_mts_default;
static struct priority_queue *global_module_timeout_queue;
static lock_t                 global_lock;

/* TODO: This function may not belong in this header file, but for now */
static inline uint64_t
module_request_queue_get_priority(void *element)
{
	struct module_global_request_queue *mgrq            = (struct module_global_request_queue *)element;
	struct sandbox_request *            sandbox_request = NULL;
	priority_queue_top_nolock(mgrq->sandbox_requests, (void **)&sandbox_request);
	return (sandbox_request) ? sandbox_request->absolute_deadline : UINT64_MAX;
};

/**
 * Promotes the given module, which means deletes from the Default queue
 *  and adds to the Guaranteed queue.
 */
void
global_request_scheduler_mts_promote_lock(struct module_global_request_queue *mgrq)
{
	assert(mgrq != NULL);
	// if(priority_queue_length_nolock(mgrq->sandbox_requests) == 0) return; // dont do anything if queue is empty

	/* No request in the given MGRQ, so it is not in the runqeue */
	// if (priority_queue_peek(mgrq->sandbox_requests) == UINT64_MAX) return;

	// assert(!LOCK_IS_LOCKED(&global_lock));
	LOCK_LOCK(&global_lock);

	if(mgrq->mt_class == MT_GUARANTEED) goto done;

	/* Delete the corresponding MGRQ from the Guaranteed queue */
	int rc = priority_queue_delete_nolock(global_request_scheduler_mts_default, mgrq);
	if (rc == -1) {
		panic("Tried to delete a non-present MGRQ from the Global Default queue. Already deleted?, ITS SIZE: "
		      "%d",
		      priority_queue_length_nolock(mgrq->sandbox_requests));
	}

	/* Add the corresponding MGRQ to the Default queue */
	rc = priority_queue_enqueue_nolock(global_request_scheduler_mts_guaranteed, mgrq);
	if (rc == -ENOSPC) panic("Global Guaranteed queue is full!\n");

done:;
	LOCK_UNLOCK(&global_lock);
}

/**
 * Demotes the given module's request queue, which means deletes the MGRQ from the Guaranteed queue
 *  and adds to the Default queue.
 */
void
global_request_scheduler_mts_demote_nolock(struct module_global_request_queue *mgrq)
{
	assert(mgrq != NULL);

	/* No request in the given MGRQ, so it is not in the runqeue */
	// if (priority_queue_peek(mgrq->sandbox_requests) == UINT64_MAX) return;

	if(mgrq->mt_class == MT_DEFAULT) return;

	/* Delete the corresponding MGRQ from the Guaranteed queue */
	int rc = priority_queue_delete_nolock(global_request_scheduler_mts_guaranteed, mgrq);
	if (rc == -1) {
		panic("Tried to delete a non-present MGRQ from the Global Guaranteed queue. Already deleted?, ITS "
		      "SIZE: %d",
		      priority_queue_length_nolock(mgrq->sandbox_requests));
	}

	/* Add the corresponding MGRQ to the Default queue */
	rc = priority_queue_enqueue_nolock(global_request_scheduler_mts_default, mgrq);
	if (rc == -ENOSPC) panic("Global Default queue is full!\n");
}

/**
 * Pushes a sandbox request to the global runqueue
 * @param sandbox_request
 * @returns pointer to request if added. NULL otherwise
 */
static struct sandbox_request *
global_request_scheduler_mts_add(struct sandbox_request *sandbox_request)
{
	assert(sandbox_request);
	assert(global_request_scheduler_mts_guaranteed && global_request_scheduler_mts_default);
	if (unlikely(!is_this_listener_thread())) panic("%s is only callable by the listener thread\n", __func__);

	struct module_global_request_queue *mgrq = sandbox_request->module->mgrq_requests;

	LOCK_LOCK(&global_lock);

	struct priority_queue *destination_queue = global_request_scheduler_mts_default;
	if (sandbox_request->module->mgrq_requests->mt_class == MT_GUARANTEED){
		destination_queue = global_request_scheduler_mts_guaranteed;
	}

	uint64_t last_mrq_deadline = priority_queue_peek(mgrq->sandbox_requests);

	int rc = priority_queue_enqueue_nolock(mgrq->sandbox_requests, sandbox_request);
	if (rc == -ENOSPC) panic("Module's Request Queue is full\n");
	// debuglog("Added a sandbox_request to the MGRQ");

	/* Maintain the minheap structure by Removing and Adding the MGRQ from and to the global runqueue.
	 * Do this only when the MGRQ's priority is updated.
	 */
	if (priority_queue_peek(mgrq->sandbox_requests) < last_mrq_deadline) {
		priority_queue_delete_nolock(destination_queue, mgrq);

		rc = priority_queue_enqueue_nolock(destination_queue, mgrq);
		if (rc == -ENOSPC) panic("Global Runqueue is full!\n");
		// debuglog("Added the MGRQ back to the Global runqueue - %s to Heapify", QUEUE_NAME);
	}

	LOCK_UNLOCK(&global_lock);

	return sandbox_request;
}

/**
 * @param pointer to the pointer that we want to set to the address of the removed sandbox request
 * @returns 0 if successful, -ENOENT if empty
 */
int
global_request_scheduler_mts_remove(struct sandbox_request **removed_sandbox_request)
{
	/* This function won't be used with the MTS scheduler. Keeping merely for the polymorhism. */
	return -1;
}


/**
 * @param removed_sandbox_request pointer to set to removed sandbox request
 * @param target_deadline the deadline that the request must be earlier than to dequeue
 * @returns 0 if successful, -ENOENT if empty or if request isn't earlier than target_deadline
 */
int
global_request_scheduler_mts_remove_if_earlier(struct sandbox_request **removed_sandbox_request,
                                               uint64_t                 target_deadline)
{
	/* This function won't be used with the MTS scheduler. Keeping merely for the polymorhism. */
	return -1;
}

/**
 * @param removed_sandbox_request pointer to set to removed sandbox request
 * @param target_deadline the deadline that the request must be earlier than to dequeue
 * @param mt_class the multi-tenancy class of the global request to compare the target deadline against
 * @returns 0 if successful, -ENOENT if empty or if request isn't earlier than target_deadline
 */
int
global_request_scheduler_mts_remove_with_mt_class(struct sandbox_request **removed_sandbox_request,
                                                  uint64_t target_deadline, enum MULTI_TENANCY_CLASS target_mt_class)
{
	/* Avoid unnessary locks when the target_deadline is tighter than the head of the Global runqueue */
	uint64_t global_guaranteed_deadline = priority_queue_peek(global_request_scheduler_mts_guaranteed);
	uint64_t global_default_deadline    = priority_queue_peek(global_request_scheduler_mts_default);

	switch (target_mt_class) {
	case MT_GUARANTEED:
		if (global_guaranteed_deadline >= target_deadline) return -ENOENT;
		break;
	case MT_DEFAULT:
		if (global_guaranteed_deadline == UINT64_MAX && global_default_deadline >= target_deadline)
			return -ENOENT;
		break;
	}

	struct module_global_request_queue *top_mgrq          = NULL;
	struct priority_queue *             destination_queue = global_request_scheduler_mts_guaranteed;

	LOCK_LOCK(&global_lock);

	/* Spot the Module Global Request Queue (MGRQ) to remove the sandbox request from */
	int rc = priority_queue_top_nolock(destination_queue, (void **)&top_mgrq);
	if (rc == -ENOENT) {
		if (target_mt_class == MT_GUARANTEED) goto done;

		destination_queue = global_request_scheduler_mts_default;

		rc = priority_queue_top_nolock(destination_queue, (void **)&top_mgrq);
		if (rc == -ENOENT) goto done;
	} else {
		if (top_mgrq->mt_class == MT_GUARANTEED && top_mgrq->module->remaining_budget <= 0) {
			global_request_scheduler_mts_demote_nolock(top_mgrq);
			// debuglog("Demoted '%s' GLOBALLY", top_mgrq->module->name);
			top_mgrq->mt_class = MT_DEFAULT;
			rc                 = -ENOENT;
			goto done;
		}
	}

	assert(top_mgrq);

	/* Remove the sandbox from the corresponding MGRQ */
	rc = priority_queue_dequeue_nolock(top_mgrq->sandbox_requests, (void **)removed_sandbox_request);
	if (rc == -ENOENT) goto done;

	/* Delete the MGRQ from the global runqueue completely if MGRQ is empty, re-add otherwise to heapify */
	if (priority_queue_delete_nolock(destination_queue, top_mgrq) == -1)
		panic("Tried to delete an MGRQ from the Global runqueue, but was not present");
	// debuglog("Removed the MGRQ from the Global runqueue");

	if (priority_queue_length_nolock(top_mgrq->sandbox_requests) > 0) {
		priority_queue_enqueue_nolock(destination_queue, top_mgrq);
		// debuglog("Added the MGRQ back to the Global runqueue to Heapify");
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
global_request_scheduler_mts_peek(void)
{
	uint64_t val = priority_queue_peek(global_request_scheduler_mts_guaranteed);
	if (val == UINT64_MAX) val = priority_queue_peek(global_request_scheduler_mts_default);

	return val;
}


uint64_t
global_request_scheduler_mts_guaranteed_peek(void)
{
	return priority_queue_peek(global_request_scheduler_mts_guaranteed);
}

uint64_t
global_request_scheduler_mts_default_peek(void)
{
	return priority_queue_peek(global_request_scheduler_mts_default);
}


/**
 * Initializes the variant and registers against the polymorphic interface
 */
void
global_request_scheduler_mts_initialize()
{
	global_request_scheduler_mts_guaranteed = priority_queue_initialize(RUNTIME_RUNQUEUE_SIZE, false,
	                                                                    module_request_queue_get_priority);
	global_request_scheduler_mts_default    = priority_queue_initialize(RUNTIME_RUNQUEUE_SIZE, false,
                                                                         module_request_queue_get_priority);

	global_module_timeout_queue = priority_queue_initialize(RUNTIME_RUNQUEUE_SIZE, false,
	                                                        module_timeout_get_priority);

	LOCK_INIT(&global_lock);

	struct global_request_scheduler_config config = {
		.add_fn                  = global_request_scheduler_mts_add,
		.remove_fn               = global_request_scheduler_mts_remove,
		.remove_if_earlier_fn    = global_request_scheduler_mts_remove_if_earlier,
		.remove_with_mt_class_fn = global_request_scheduler_mts_remove_with_mt_class,
		.peek_fn                 = global_request_scheduler_mts_peek
	};

	global_request_scheduler_initialize(&config);
}

void
global_request_scheduler_mts_free()
{
	priority_queue_free(global_request_scheduler_mts_guaranteed);
	priority_queue_free(global_request_scheduler_mts_default);
	priority_queue_free(global_module_timeout_queue);
}


void
global_timeout_queue_add(struct module *module)
{
	module->mgrq_requests->module_timeout.timeout = get_next_timeout_of_module(module->replenishment_period);
	priority_queue_enqueue_nolock(global_module_timeout_queue, &module->mgrq_requests->module_timeout);
}

/*
 * Checks if there are any tenant timers that run out in the GLOBAL queue,
 *  if so promote that tenant.
 */
void
global_timeout_queue_check_for_promotions()
{
	uint64_t now = __getcycles();

	struct module_timeout *top_module_timeout = NULL;

	/* Check the timeout queue for a potential tenant to get PRomoted */
	priority_queue_top_nolock(global_module_timeout_queue, (void **)&top_module_timeout);

	if (top_module_timeout == NULL || now < top_module_timeout->timeout) return;

	struct module *                     module          = top_module_timeout->module;
	struct module_global_request_queue *mgrq_to_promote = module->mgrq_requests;

	if (mgrq_to_promote->mt_class == MT_DEFAULT) {
		global_request_scheduler_mts_promote_lock(mgrq_to_promote);
		mgrq_to_promote->mt_class = MT_GUARANTEED;
		// debuglog("Promoted '%s' GLOBALLY", module->name);
	}

	// TODO: We need a smarter technique to reset budget to consider budget overusage:
	// module->remaining_budget = module->max_budget;
	int64_t prev_budget = atomic_load(&module->remaining_budget);
	while(!atomic_compare_exchange_strong(&module->remaining_budget, &prev_budget, module->max_budget));

	/* Reheapify the timeout queue with the updated timeout value of the module */
	priority_queue_delete_nolock(global_module_timeout_queue, top_module_timeout);
	top_module_timeout->timeout = get_next_timeout_of_module(module->replenishment_period);
	// top_module_timeout->timeout += module->replenishment_period;
	priority_queue_enqueue_nolock(global_module_timeout_queue, top_module_timeout);
}
