#include "global_request_scheduler.h"


/* The global of our polymorphic interface */
static struct global_request_scheduler_config global_request_scheduler;

/**
 * Initializes the polymorphic interface with a concrete implementation
 * @param config
 */
void
global_request_scheduler_initialize(struct global_request_scheduler_config *config)
{
	memcpy(&global_request_scheduler, config, sizeof(struct global_request_scheduler_config));
}


/**
 * Adds a sandbox request to the request scheduler
 * @param sandbox_request
 */
sandbox_request_t *
global_request_scheduler_add(sandbox_request_t *sandbox_request)
{
	assert(global_request_scheduler.add_fn != NULL);
	return global_request_scheduler.add_fn(sandbox_request);
}

/**
 * Removes a sandbox request according to the scheduling policy of the variant
 * @returns pointer to a sandbox request
 */
int
global_request_scheduler_remove(sandbox_request_t **removed_sandbox)
{
	assert(global_request_scheduler.remove_fn != NULL);
	return global_request_scheduler.remove_fn(removed_sandbox);
}

/**
 * Peeks at the priority of the highest priority sandbox request
 * @returns highest priority
 */
uint64_t
global_request_scheduler_peek()
{
	assert(global_request_scheduler.peek_fn != NULL);
	return global_request_scheduler.peek_fn();
};
