#include <sandbox_run_queue.h>

static sandbox_run_queue_config_t sandbox_run_queue;

/* Initializes a concrete implementation of the sandbox request scheduler interface */
void
sandbox_run_queue_initialize(sandbox_run_queue_config_t *config)
{
	memcpy(&sandbox_run_queue, config, sizeof(sandbox_run_queue_config_t));
}

/**
 * Adds a sandbox request to the run queue
 * @param sandbox to add
 * @returns sandbox that was added (or NULL?)
 */
struct sandbox *
sandbox_run_queue_add(struct sandbox *sandbox)
{
	assert(sandbox_run_queue.add != NULL);
	return sandbox_run_queue.add(sandbox);
}

/**
 * Delete a sandbox from the run queue
 * @param sandbox to delete
 */
void
sandbox_run_queue_delete(struct sandbox *sandbox)
{
	assert(sandbox_run_queue.delete != NULL);
	sandbox_run_queue.delete(sandbox);
}

/**
 * Checks if run queue is empty
 * @returns true if empty
 */
bool
sandbox_run_queue_is_empty()
{
	assert(sandbox_run_queue.is_empty != NULL);
	return sandbox_run_queue.is_empty();
}

/**
 * Get next sandbox from run queue, where next is defined by
 * @returns sandbox (or NULL?)
 */
struct sandbox *
sandbox_run_queue_get_next()
{
	assert(sandbox_run_queue.get_next != NULL);
	return sandbox_run_queue.get_next();
};

/**
 * Preempt the current sandbox according to the scheduler variant
 * @param context
 */
void
sandbox_run_queue_preempt(ucontext_t *context)
{
	assert(sandbox_run_queue.preempt != NULL);
	return sandbox_run_queue.preempt(context);
};