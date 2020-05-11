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

void
sandbox_run_queue_delete(struct sandbox *sandbox)
{
	assert(sandbox_run_queue.delete != NULL);
	sandbox_run_queue.delete(sandbox);
}


bool
sandbox_run_queue_is_empty()
{
	assert(sandbox_run_queue.is_empty != NULL);
	return sandbox_run_queue.is_empty();
}

struct sandbox *
sandbox_run_queue_get_next()
{
	assert(sandbox_run_queue.get_next != NULL);
	return sandbox_run_queue.get_next();
};

void
sandbox_run_queue_preempt(ucontext_t *context)
{
	assert(sandbox_run_queue.preempt != NULL);
	return sandbox_run_queue.preempt(context);
};