#include <sandbox_request_scheduler.h>
#include "priority_queue.h"

// Local State
static struct priority_queue sandbox_request_scheduler_ps;

/**
 * Pushes a sandbox request to the global deque
 * @param sandbox_request
 * @returns pointer to request if added. NULL otherwise
 **/
static sandbox_request_t *
sandbox_request_scheduler_ps_add(void *sandbox_request_raw)
{
	// TODO
	return NULL;
}

/**
 *
 * @returns A Sandbox Request or NULL
 **/
static sandbox_request_t *
sandbox_request_scheduler_ps_remove(void)
{
	// TODO
	return NULL;
}

/**
 *
 **/
void
sandbox_request_scheduler_ps_initialize()
{
	// Initialize local state

	// TODO


	// Register Function Pointers for Abstract Scheduling API
	sandbox_request_scheduler_config_t config = { .add    = sandbox_request_scheduler_ps_add,
		                                      .remove = sandbox_request_scheduler_ps_remove };

	sandbox_request_scheduler_initialize(&config);
}