#include <sandbox_run_queue.h>


// The global of our polymorphic interface
static sandbox_run_queue_config_t sandbox_run_queue;

// Initializes a concrete implementation of the sandbox request scheduler interface
void
sandbox_run_queue_initialize(sandbox_run_queue_config_t *config)
{
	memcpy(&sandbox_run_queue, config, sizeof(sandbox_run_queue_config_t));
}
// Adds a sandbox request
struct sandbox *
sandbox_run_queue_add(struct sandbox *sandbox)
{
	assert(sandbox_run_queue.add != NULL);
	return sandbox_run_queue.add(sandbox);
}

// Removes a sandbox request
struct sandbox *
sandbox_run_queue_remove()
{
	assert(sandbox_run_queue.remove != NULL);
	return sandbox_run_queue.remove();
}

bool
sandbox_run_queue_is_empty()
{
	return sandbox_run_queue_is_empty();
}