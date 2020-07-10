#include "global_request_scheduler.h"
#include "panic.h"
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
	int return_code = priority_queue_enqueue(&global_request_scheduler_minheap, sandbox_request);
	/* TODO: Propagate -1 to caller */
	if (return_code == -1) panic("Request Queue is full\n");
	return sandbox_request;
}

/**
 * @param pointer to the pointer that we want to set to the address of the removed sandbox request
 * @returns 0 if successful, -1 if empty, -2 if unable to take lock or perform atomic operation
 */
int
global_request_scheduler_minheap_remove(sandbox_request_t **removed_sandbox_request)
{
	return priority_queue_dequeue(&global_request_scheduler_minheap, (void **)removed_sandbox_request);
}

/**
 * Peek at the priority of the highest priority task without having to take the lock
 * Because this is a min-heap PQ, the highest priority is the lowest 64-bit integer
 * This is used to store an absolute deadline
 * @returns value of highest priority value in queue or ULONG_MAX if empty
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
