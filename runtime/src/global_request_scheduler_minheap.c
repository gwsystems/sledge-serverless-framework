#include <global_request_scheduler.h>
#include "priority_queue.h"

static struct priority_queue global_request_scheduler_minheap;

/**
 * Pushes a sandbox request to the global deque
 * @param sandbox_request
 * @returns pointer to request if added. NULL otherwise
 */
static sandbox_request_t *
global_request_scheduler_minheap_add(void *sandbox_request)
{
	int return_code = priority_queue_enqueue(&global_request_scheduler_minheap, sandbox_request, "Request");

	if (return_code == -1) {
		printf("Request Queue is full\n");
		exit(EXIT_FAILURE);
	}

	if (return_code != 0) return NULL;
	return sandbox_request;
}

/**
 *
 * @returns A Sandbox Request or NULL
 */
static sandbox_request_t *
global_request_scheduler_minheap_remove(void)
{
	return (sandbox_request_t *)priority_queue_dequeue(&global_request_scheduler_minheap, "Request");
}

/**
 *
 * @returns A Sandbox Request or NULL
 */
static uint64_t
global_request_scheduler_minheap_peek(void)
{
	return priority_queue_peek(&global_request_scheduler_minheap);
}

uint64_t
sandbox_request_get_priority_fn(void *element)
{
	sandbox_request_t *sandbox_request = (sandbox_request_t *)element;
	return sandbox_request->absolute_deadline;
};


/**
 * Initializes the variant and registers against the polymorphic interface
 */
void
global_request_scheduler_minheap_initialize()
{
	priority_queue_initialize(&global_request_scheduler_minheap, sandbox_request_get_priority_fn);

	struct global_request_scheduler_config config = { .add_fn    = global_request_scheduler_minheap_add,
		                                          .remove_fn = global_request_scheduler_minheap_remove,
		                                          .peek_fn   = global_request_scheduler_minheap_peek };

	global_request_scheduler_initialize(&config);
}