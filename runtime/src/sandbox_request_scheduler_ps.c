#include <sandbox_request_scheduler.h>
#include "priority_queue.h"

static struct priority_queue sandbox_request_scheduler_ps;

/**
 * Pushes a sandbox request to the global deque
 * @param sandbox_request
 * @returns pointer to request if added. NULL otherwise
 */
static sandbox_request_t *
sandbox_request_scheduler_ps_add(void *sandbox_request)
{
	int return_code = priority_queue_enqueue(&sandbox_request_scheduler_ps, sandbox_request, "Request");

	if (return_code == -1) {
		printf("Request Queue is full\n");
		exit(EXIT_FAILURE);
	}

	return return_code == 0 ? sandbox_request : NULL;
}

/**
 *
 * @returns A Sandbox Request or NULL
 */
static sandbox_request_t *
sandbox_request_scheduler_ps_remove(void)
{
	return (sandbox_request_t *)priority_queue_dequeue(&sandbox_request_scheduler_ps, "Request");
}

/**
 *
 * @returns A Sandbox Request or NULL
 */
static uint64_t
sandbox_request_scheduler_ps_peek(void)
{
	return priority_queue_peek(&sandbox_request_scheduler_ps);
}

uint64_t
sandbox_request_get_priority(void *element)
{
	sandbox_request_t *sandbox_request = (sandbox_request_t *)element;
	return sandbox_request->absolute_deadline;
};


/**
 * Initializes the variant and registers against the polymorphic interface
 */
void
sandbox_request_scheduler_ps_initialize()
{
	priority_queue_initialize(&sandbox_request_scheduler_ps, sandbox_request_get_priority);

	sandbox_request_scheduler_config_t config = { .add    = sandbox_request_scheduler_ps_add,
		                                      .remove = sandbox_request_scheduler_ps_remove,
		                                      .peek   = sandbox_request_scheduler_ps_peek };

	sandbox_request_scheduler_initialize(&config);
}