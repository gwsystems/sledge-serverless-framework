#include "local_runqueue.h"

static struct local_runqueue_config local_runqueue;

/* Initializes a concrete implementation of the sandbox request scheduler interface */
void
local_runqueue_initialize(struct local_runqueue_config *config)
{
	memcpy(&local_runqueue, config, sizeof(struct local_runqueue_config));
}

/**
 * Adds a sandbox request to the run queue
 * @param sandbox to add
 * @returns sandbox that was added (or NULL?)
 */
struct sandbox *
local_runqueue_add(struct sandbox *sandbox)
{
	assert(local_runqueue.add_fn != NULL);
	return local_runqueue.add_fn(sandbox);
}

/**
 * Delete a sandbox from the run queue
 * @param sandbox to delete
 */
void
local_runqueue_delete(struct sandbox *sandbox)
{
	assert(local_runqueue.delete_fn != NULL);
	local_runqueue.delete_fn(sandbox);
}

/**
 * Checks if run queue is empty
 * @returns true if empty
 */
bool
local_runqueue_is_empty()
{
	assert(local_runqueue.is_empty_fn != NULL);
	return local_runqueue.is_empty_fn();
}

/**
 * Get next sandbox from run queue, where next is defined by
 * @returns sandbox (or NULL?)
 */
struct sandbox *
local_runqueue_get_next()
{
	assert(local_runqueue.get_next_fn != NULL);
	return local_runqueue.get_next_fn();
};

/**
 * Preempt the current sandbox according to the scheduler variant
 * @param context
 */
void
local_runqueue_preempt(ucontext_t *context)
{
	assert(local_runqueue.preempt_fn != NULL);
	return local_runqueue.preempt_fn(context);
};