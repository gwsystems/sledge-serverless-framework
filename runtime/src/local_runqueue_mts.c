#include <stdint.h>

#include "arch/context.h"
#include "client_socket.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "global_request_scheduler.h"
#include "local_runqueue.h"
#include "local_runqueue_mts.h"
#include "panic.h"
#include "priority_queue.h"
#include "sandbox_functions.h"
#include "runtime.h"

__thread static struct priority_queue *local_runqueue_mts_guaranteed;
__thread static struct priority_queue *local_runqueue_mts_default;

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
	struct sandbox *                       sandbox = NULL;
	priority_queue_top_nolock(pwm->sandboxes, (void **)&sandbox);
	return (sandbox) ? sandbox->absolute_deadline : UINT64_MAX;
}

/**
 * Checks if the run queue is empty
 * @returns true if empty. false otherwise
 */
bool
local_runqueue_mts_is_empty()
{
	return priority_queue_length_nolock(local_runqueue_mts_guaranteed) == 0
	       && priority_queue_length_nolock(local_runqueue_mts_default) == 0;
}

/**
 * Adds a sandbox to the run queue
 * @param sandbox
 * @returns pointer to sandbox added
 */
void
local_runqueue_mts_add(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	struct perworker_module_sandbox_queue *pwm               = &sandbox->module->pwm_sandboxes[worker_thread_idx];
	struct priority_queue *                destination_queue = local_runqueue_mts_default;

	uint64_t prev_pwm_deadline = priority_queue_peek(pwm->sandboxes);

	/* Add the sandbox to the per-worker-module (pwm) queue */
	int rc = priority_queue_enqueue_nolock(pwm->sandboxes, sandbox);
	if (rc == -ENOSPC) panic("Per Worker Module queue is full!\n");
	// debuglog("Added a sandbox to the PWM of module '%s'", sandbox->module->name);

	if (priority_queue_length_nolock(pwm->sandboxes) == 1) {
		pwm->mt_class = MT_DEFAULT;
		if (sandbox->module->remaining_budget > 0) {
			pwm->mt_class     = MT_GUARANTEED;
			destination_queue = local_runqueue_mts_guaranteed;
		}

		/* Add sandbox module to the worker's timeout queue if guaranteed tenant and only if first sandbox*/
		if (sandbox->module->replenishment_period > 0) {
			struct module_timeout *mt = malloc(sizeof(struct module_timeout));

			mt->timeout = get_next_timeout_of_module(sandbox->module->replenishment_period, __getcycles());
			mt->module  = sandbox->module;
			priority_queue_enqueue_nolock(worker_thread_timeout_queue, mt);
		}

		rc = priority_queue_enqueue_nolock(destination_queue, pwm);

		return;
	}

	if (sandbox->absolute_deadline < prev_pwm_deadline) {
		/* Maintain the minheap structure by Deleting and
		 *  adding the pwm from and to the worker's local runqueue.
		 * Do this only when the pwm's priority is updated. */
		if (pwm->mt_class == MT_GUARANTEED) destination_queue = local_runqueue_mts_guaranteed;

		rc = priority_queue_delete_nolock(destination_queue, pwm);
		assert(rc == 0);

		rc = priority_queue_enqueue_nolock(destination_queue, pwm);
		if (rc == -ENOSPC) panic("Worker Local Runqueue is full!\n");

		// debuglog("Added the PWM back to the Worker Local runqueue - %s to Heapify", QUEUE_NAME);
	}
}

/**
 * Deletes a sandbox from the runqueue
 * @param sandbox to delete
 */
static void
local_runqueue_mts_delete(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	struct perworker_module_sandbox_queue *pwm = &sandbox->module->pwm_sandboxes[worker_thread_idx];

	/* Delete the sandbox from the corresponding Per-Worker-Module queue */
	if (priority_queue_delete_nolock(pwm->sandboxes, sandbox) == -1)
		panic("Tried to delete sandbox %lu from PWM queue, but was not present\n", sandbox->id);

	struct priority_queue *destination_queue = local_runqueue_mts_default;
	/* Because of the race, the following if clause does not work properly, so just trying both queues */
	/*
	if (sandbox->module->remaining_budget > 0) {
	        destination_queue = local_runqueue_mts_guaranteed;
	}
	*/

	/* Delete the PWM from the local runqueue completely if pwm is empty, re-add otherwise to heapify */
	if (priority_queue_delete_nolock(destination_queue, pwm) == -1) {
		// TODO: Apply the composite way of PQ deletion O(logn)
		destination_queue = local_runqueue_mts_guaranteed;

		if (priority_queue_delete_nolock(destination_queue, pwm) == -1)
			panic("Tried to delete a PWM of %s from local runqueue, but was not present\n",
			      pwm->module->name);
	}
	// debuglog("Deleted the PWM from the Worker Local runqueue - %s to Heapify", QUEUE_NAME);

	/* Add the PWM back to the local runqueue if it still has other sandboxes inside */
	if (priority_queue_length_nolock(pwm->sandboxes) > 0) {
		priority_queue_enqueue_nolock(destination_queue, pwm);
		// debuglog("Added the PWM back to the Worker Local runqueue - %s to Heapify", QUEUE_NAME);
	} else if (pwm->module->replenishment_period > 0) {
		struct module_timeout *dummy = NULL;
		priority_queue_dequeue_nolock(worker_thread_timeout_queue, (void **)&dummy);
		assert(dummy != NULL);
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
local_runqueue_mts_get_next()
{
	/* Get the deadline of the sandbox at the head of the local request queue */
	struct perworker_module_sandbox_queue *next_pwm = NULL;
	struct priority_queue *                dq       = local_runqueue_mts_guaranteed;

	int rc = priority_queue_top_nolock(dq, (void **)&next_pwm);
	while (rc != -ENOENT && next_pwm->module->remaining_budget <= 0) {
		local_runqueue_mts_demote(next_pwm);
		debuglog("Demoted '%s' locally", next_pwm->module->name);
		next_pwm->mt_class = MT_DEFAULT;

		rc = priority_queue_top_nolock(dq, (void **)&next_pwm);
	}


	if (rc == -ENOENT) {
		dq = local_runqueue_mts_default;
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
local_runqueue_mts_initialize()
{
	/* Initialize local state */
	local_runqueue_mts_guaranteed = priority_queue_initialize(RUNTIME_RUNQUEUE_SIZE, false,
	                                                                  perworker_module_get_priority);
	local_runqueue_mts_default    = priority_queue_initialize(RUNTIME_RUNQUEUE_SIZE, false,
                                                                       perworker_module_get_priority);

	/* Register Function Pointers for Abstract Scheduling API */
	struct local_runqueue_config config = { .add_fn      = local_runqueue_mts_add,
		                                .is_empty_fn = local_runqueue_mts_is_empty,
		                                .delete_fn   = local_runqueue_mts_delete,
		                                .get_next_fn = local_runqueue_mts_get_next };

	local_runqueue_initialize(&config);
}

/**
 * Promotes the given per-worker-module queue, which means deletes from the Default queue
 *  and adds to the Guaranteed queue.
 */
void
local_runqueue_mts_promote(struct perworker_module_sandbox_queue *pwm)
{
	assert(pwm != NULL);

	/* Delete the corresponding PWM from the Guaranteed queue */
	int rc = priority_queue_delete_nolock(local_runqueue_mts_default, pwm);
	if (rc == -1) {
		panic("Tried to delete a non-present PWM of %s from the Local Default queue. Already deleted? , ITS "
		      "SIZE: %d",
		      pwm->module->name, priority_queue_length_nolock(pwm->sandboxes));
	}

	/* Add the corresponding PWM to the Default queue */
	rc = priority_queue_enqueue_nolock(local_runqueue_mts_guaranteed, pwm);
	if (rc == -ENOSPC) panic("Local Guaranteed queue is full!\n");
}

/**
 * Demotes the given per-worker-module queue, which means deletes from the Guaranteed queue
 *  and adds to the Default queue.
 */
void
local_runqueue_mts_demote(struct perworker_module_sandbox_queue *pwm)
{
	assert(pwm != NULL);

	/* Delete the corresponding PWM from the Guaranteed queue */
	int rc = priority_queue_delete_nolock(local_runqueue_mts_guaranteed, pwm);
	if (rc == -1) {
		panic("Tried to delete a non-present PWM of %s from the Local Guaranteed queue. Already deleted? , ITS "
		      "SIZE: %d",
		      pwm->module->name, priority_queue_length_nolock(pwm->sandboxes));
	}

	/* Add the corresponding PWM to the Default queue */
	rc = priority_queue_enqueue_nolock(local_runqueue_mts_default, pwm);
	if (rc == -ENOSPC) panic("Local Default queue is full!\n");
}


/*
 * Checks if there are any tenant timers that run out in the LOCAL queue,
 *  if so promote that tenant.
 */
void
local_timeout_queue_check_for_promotions(uint64_t now)
{
	struct module_timeout *top_module_timeout = NULL;

	/* Check the timeout queue for a potential tenant to get PRomoted */
	priority_queue_top_nolock(worker_thread_timeout_queue, (void **)&top_module_timeout);

	if (top_module_timeout == NULL || now < top_module_timeout->timeout) return;

	struct perworker_module_sandbox_queue *pwm_to_promote =
	  &top_module_timeout->module->pwm_sandboxes[worker_thread_idx];

	if (pwm_to_promote->mt_class == MT_DEFAULT /*&& priority_queue_length_nolock(pwm_to_promote->sandboxes) > 0*/) {
		local_runqueue_mts_promote(pwm_to_promote);
		pwm_to_promote->mt_class = MT_GUARANTEED;

		debuglog("Promoted '%s' locally", top_module_timeout->module->name);
	}


	/* Reheapify the timeout queue with the updated timeout value of the module */
	priority_queue_delete_nolock(worker_thread_timeout_queue, top_module_timeout);
	top_module_timeout->timeout = get_next_timeout_of_module(top_module_timeout->module->replenishment_period, now);
	priority_queue_enqueue_nolock(worker_thread_timeout_queue, top_module_timeout);
}
