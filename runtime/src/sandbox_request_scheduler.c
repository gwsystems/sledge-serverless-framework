#include <sandbox_request_scheduler.h>


// The global of our polymorphic interface
static sandbox_request_scheduler_config_t sandbox_request_scheduler;

// Initializes a concrete implementation of the sandbox request scheduler interface
void
sandbox_request_scheduler_initialize(sandbox_request_scheduler_config_t *config)
{
	memcpy(&sandbox_request_scheduler, config, sizeof(sandbox_request_scheduler_config_t));
}

// Adds a sandbox request
sandbox_request_t *
sandbox_request_scheduler_add(sandbox_request_t *sandbox_request)
{
	assert(sandbox_request_scheduler.add != NULL);
	return sandbox_request_scheduler.add(sandbox_request);
}

// Removes a sandbox request
sandbox_request_t *
sandbox_request_scheduler_remove()
{
	assert(sandbox_request_scheduler.remove != NULL);
	return sandbox_request_scheduler.remove();
}
