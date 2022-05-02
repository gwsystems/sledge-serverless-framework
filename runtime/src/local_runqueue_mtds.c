#include <stdint.h>
#include <threads.h>

#include "arch/context.h"
#include "client_socket.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "global_request_scheduler.h"
#include "local_runqueue.h"
#include "local_runqueue_mtds.h"
#include "panic.h"
#include "priority_queue.h"
#include "sandbox_functions.h"
#include "runtime.h"

thread_local static struct priority_queue *local_runqueue_mtds_guaranteed;
thread_local static struct priority_queue *local_runqueue_mtds_default;

extern __thread struct priority_queue *worker_thread_timeout_queue;

/**
 * Get Per-Worker-Module priority for Priority Queue ordering
 * @param element the PWM
 * @returns the priority of the head of the PWM
 */
static inline uint64_t
perworker_module_get_priority(void *element)
{
	struct perworker_module_sandbox_queue *pwm     = (struct perworker_module_sandbox_queue *)element;
	struct sandbox                        *sandbox = NULL;
	priority_queue_top_nolock(pwm->sandboxes, (void **)&sandbox);
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

	struct perworker_module_sandbox_queue *pwm = &sandbox->module->pwm_sandboxes[worker_thread_idx];
	struct priority_queue *destination_queue   = pwm->mt_class == MT_GUARANTEED ? local_runqueue_mtds_guaranteed
	                                                                            : local_runqueue_mtds_default;

	uint64_t prev_pwm_deadline = priority_queue_peek(pwm->sandboxes);

	/* Add the sandbox to the per-worker-module (pwm) queue */
	int rc = priority_queue_enqueue_nolock(pwm->sandboxes, sandbox);
	if (rc == -ENOSPC) panic("Per Worker Module queue is full!\n");
	// debuglog("Added a sandbox to the PWM of module '%s'", sandbox->module->name);

	/* If the module was not in the local runqueue, then since we are the first sandbox for it in this worker, add
	 * the module.  */
	if (priority_queue_length_nolock(pwm->sandboxes) == 1) {
		/* Add sandbox module to the worker's timeout queue if guaranteed tenant and only if first sandbox*/
		if (module_is_paid(sandbox->module)) {
			pwm->module_timeout.timeout = get_next_timeout_of_module(sandbox->module->replenishment_period);
			priority_queue_enqueue_nolock(worker_thread_timeout_queue, &pwm->module_timeout);
		}

		rc = priority_queue_enqueue_nolock(destination_queue, pwm);

		return;
	}

	if (sandbox->absolute_deadline < prev_pwm_deadline) {
		/* Maintain the minheap structure by deleting & adding the pwm.
		 * Do this only when the pwm's priority is updated. */
		rc = priority_queue_delete_nolock(destination_queue, pwm);
		assert(rc == 0);
		rc = priority_queue_enqueue_nolock(destination_queue, pwm);
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
	struct perworker_module_sandbox_queue *pwm = &sandbox->module->pwm_sandboxes[worker_thread_idx];

	/* Delete the sandbox from the corresponding Per-Worker-Module queue */
	if (priority_queue_delete_nolock(pwm->sandboxes, sandbox) == -1) {
		panic("Tried to delete sandbox %lu from PWM queue, but was not present\n", sandbox->id);
	}

	struct priority_queue *destination_queue = local_runqueue_mtds_default;

	if (pwm->mt_class == MT_GUARANTEED) { destination_queue = local_runqueue_mtds_guaranteed; }


	/* Delete the PWM from the local runqueue completely if pwm is empty, re-add otherwise to heapify */
	if (priority_queue_delete_nolock(destination_queue, pwm) == -1) {
		// TODO: Apply the composite way of PQ deletion O(logn)
		panic("Tried to delete a PWM of %s from local runqueue, but was not present\n", pwm->module->name);
	}

	/* Add the PWM back to the local runqueue if it still has other sandboxes inside */
	if (priority_queue_length_nolock(pwm->sandboxes) > 0) {
		priority_queue_enqueue_nolock(destination_queue, pwm);
	} else if (module_is_paid(pwm->module)) {
		priority_queue_delete_nolock(worker_thread_timeout_queue, &pwm->module_timeout);
		pwm->mt_class = MT_GUARANTEED;
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
	struct perworker_module_sandbox_queue *next_pwm = NULL;
	struct priority_queue                 *dq       = local_runqueue_mtds_guaranteed;

	/* Check the local guaranteed queue for any potential demotions */
	int rc = priority_queue_top_nolock(dq, (void **)&next_pwm);
	while (rc != -ENOENT && next_pwm->module->remaining_budget <= 0) { // next_pwm->mt_class==MT_DEFAULT){
		local_runqueue_mtds_demote(next_pwm);
		// debuglog("Demoted '%s' locally in GetNext", next_pwm->module->name);
		next_pwm->mt_class = MT_DEFAULT;
		rc                 = priority_queue_top_nolock(dq, (void **)&next_pwm);
	}

	if (rc == -ENOENT) {
		dq = local_runqueue_mtds_default;
		rc = priority_queue_top_nolock(dq, (void **)&next_pwm);
		if (rc == -ENOENT) return NULL;
	}

	struct sandbox *next_sandbox = NULL;
	priority_queue_top_nolock(next_pwm->sandboxes, (void **)&next_sandbox);
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
	local_runqueue_mtds_guaranteed = priority_queue_initialize(RUNTIME_RUNQUEUE_SIZE, false,
	                                                           perworker_module_get_priority);
	local_runqueue_mtds_default    = priority_queue_initialize(RUNTIME_RUNQUEUE_SIZE, false,
	                                                           perworker_module_get_priority);

	/* Register Function Pointers for Abstract Scheduling API */
	struct local_runqueue_config config = { .add_fn      = local_runqueue_mtds_add,
		                                .is_empty_fn = local_runqueue_mtds_is_empty,
		                                .delete_fn   = local_runqueue_mtds_delete,
		                                .get_next_fn = local_runqueue_mtds_get_next };

	local_runqueue_initialize(&config);
}

/**
 * Promotes the given per-worker-module queue, which means deletes from the Default queue
 *  and adds to the Guaranteed queue.
 */
void
local_runqueue_mtds_promote(struct perworker_module_sandbox_queue *pwm)
{
	assert(pwm != NULL);

	/* Delete the corresponding PWM from the Guaranteed queue */
	int rc = priority_queue_delete_nolock(local_runqueue_mtds_default, pwm);
	if (rc == -1) {
		panic("Tried to delete a non-present PWM of %s from the Local Default queue. Already deleted? , ITS "
		      "SIZE: %d",
		      pwm->module->name, priority_queue_length_nolock(pwm->sandboxes));
	}

	/* Add the corresponding PWM to the Default queue */
	rc = priority_queue_enqueue_nolock(local_runqueue_mtds_guaranteed, pwm);
	if (rc == -ENOSPC) panic("Local Guaranteed queue is full!\n");
}

/**
 * Demotes the given per-worker-module queue, which means deletes from the Guaranteed queue
 *  and adds to the Default queue.
 */
void
local_runqueue_mtds_demote(struct perworker_module_sandbox_queue *pwm)
{
	assert(pwm != NULL);

	/* Delete the corresponding PWM from the Guaranteed queue */
	int rc = priority_queue_delete_nolock(local_runqueue_mtds_guaranteed, pwm);
	if (rc == -1) {
		panic("Tried to delete a non-present PWM of %s from the Local Guaranteed queue. Already deleted? , ITS "
		      "SIZE: %d",
		      pwm->module->name, priority_queue_length_nolock(pwm->sandboxes));
	}

	/* Add the corresponding PWM to the Default queue */
	rc = priority_queue_enqueue_nolock(local_runqueue_mtds_default, pwm);
	if (rc == -ENOSPC) panic("Local Default queue is full!\n");
}


/*
 * Checks if there are any tenant timers that run out in the LOCAL queue,
 *  if so promote that tenant.
 */
void
local_timeout_queue_process_promotions()
{
	struct module_timeout *top_module_timeout = NULL;

	/* Check the timeout queue for a potential tenant to get PRomoted */
	priority_queue_top_nolock(worker_thread_timeout_queue, (void **)&top_module_timeout);
	if (top_module_timeout == NULL) return; // no guaranteed tenants

	struct perworker_module_sandbox_queue *pwm_to_promote = NULL;
	uint64_t                               now            = __getcycles();

	while (now >= top_module_timeout->timeout) {
		pwm_to_promote = top_module_timeout->pwm;
		assert(priority_queue_length_nolock(pwm_to_promote->sandboxes) > 0);

		if (pwm_to_promote->mt_class == MT_DEFAULT) {
			local_runqueue_mtds_promote(pwm_to_promote);
			pwm_to_promote->mt_class = MT_GUARANTEED;
			// debuglog("Promoted '%s' locally", top_module_timeout->module->name);
		}

		/* Reheapify the timeout queue with the updated timeout value of the module */
		priority_queue_delete_nolock(worker_thread_timeout_queue, top_module_timeout);
		top_module_timeout->timeout = get_next_timeout_of_module(
		  top_module_timeout->module->replenishment_period);
		priority_queue_enqueue_nolock(worker_thread_timeout_queue, top_module_timeout);

		priority_queue_top_nolock(worker_thread_timeout_queue, (void **)&top_module_timeout);
	}
}
