#include "sandbox_run_queue_ps.h"
#include "sandbox_run_queue.h"
#include "priority_queue.h"

// Local State
__thread static struct priority_queue sandbox_run_queue_ps;

bool
sandbox_run_queue_ps_is_empty()
{
	return priority_queue_length(&sandbox_run_queue_ps) == 0;
}

/**
 * Pushes a sandbox to the runqueue
 * @param sandbox
 * @returns pointer to request if added. NULL otherwise
 **/
static struct sandbox *
sandbox_run_queue_ps_add(struct sandbox *sandbox)
{
	int return_code = priority_queue_enqueue(&sandbox_run_queue_ps, sandbox);

	return return_code == 0 ? sandbox : NULL;
}

/**
 *
 * @returns A Sandbox Request or NULL
 **/
static struct sandbox *
sandbox_run_queue_ps_remove(void)
{
	return (struct sandbox *)priority_queue_dequeue(&sandbox_run_queue_ps);
}

unsigned long long int
sandbox_get_priority(void *element)
{
	struct sandbox *sandbox = (struct sandbox *)element;
	return sandbox->absolute_deadline;
};

void
sandbox_run_queue_ps_initialize()
{
	// Initialize local state
	priority_queue_initialize(&sandbox_run_queue_ps, sandbox_get_priority);

	// Register Function Pointers for Abstract Scheduling API
	sandbox_run_queue_config_t config = { .add      = sandbox_run_queue_ps_add,
		                              .is_empty = sandbox_run_queue_ps_is_empty,
		                              .remove   = sandbox_run_queue_ps_remove };

	sandbox_run_queue_initialize(&config);
}
