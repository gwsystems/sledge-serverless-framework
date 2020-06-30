#include <sandbox_request_scheduler.h>


/* The global of our polymorphic interface */
static sandbox_request_scheduler_config_t sandbox_request_scheduler;

/**
 * Initializes the polymorphic interface with a concrete implementation
 * @param config
 */
void
sandbox_request_scheduler_initialize(sandbox_request_scheduler_config_t *config)
{
	memcpy(&sandbox_request_scheduler, config, sizeof(sandbox_request_scheduler_config_t));
}


/**
 * Adds a sandbox request to the request scheduler
 * @param sandbox_request
 */
sandbox_request_t *
sandbox_request_scheduler_add(sandbox_request_t *sandbox_request)
{
	assert(sandbox_request_scheduler.add != NULL);
	return sandbox_request_scheduler.add(sandbox_request);
}

/**
 * Removes a sandbox request according to the scheduling policy of the variant
 * @returns pointer to a sandbox request
 */
sandbox_request_t *
sandbox_request_scheduler_remove()
{
	assert(sandbox_request_scheduler.remove != NULL);
	return sandbox_request_scheduler.remove();
}

/**
 * Peeks at the priority of the highest priority sandbox request
 * @returns highest priority
 */
uint64_t
sandbox_request_scheduler_peek()
{
	assert(sandbox_request_scheduler.peek != NULL);
	return sandbox_request_scheduler.peek();
};