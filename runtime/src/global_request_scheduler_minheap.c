#include <assert.h>

#include "global_request_scheduler.h"
#include "panic.h"
#include "priority_queue.h"
#include "runtime.h"

static struct priority_queue *global_request_scheduler_minheap;

/**
 * Pushes a sandbox request to the global deque
 * @param sandbox_request
 * @returns pointer to request if added. NULL otherwise
 */
static struct sandbox_request *
global_request_scheduler_minheap_add(void *sandbox_request)
{
	assert(sandbox_request);
	assert(global_request_scheduler_minheap);
	if (unlikely(runtime_is_worker())) panic("%s is only callable by the listener thread\n", __func__);

	int return_code = priority_queue_enqueue(global_request_scheduler_minheap, sandbox_request);
	/* TODO: Propagate -1 to caller. Issue #91 */
	if (return_code == -ENOSPC) panic("Request Queue is full\n");
	return sandbox_request;
}

/**
 * @param pointer to the pointer that we want to set to the address of the removed sandbox request
 * @returns 0 if successful, -ENOENT if empty
 */
int
global_request_scheduler_minheap_remove(struct sandbox_request **removed_sandbox_request)
{
	assert(!software_interrupt_is_enabled());
	return priority_queue_dequeue(global_request_scheduler_minheap, (void **)removed_sandbox_request);
}

/**
 * @param removed_sandbox_request pointer to set to removed sandbox request
 * @param target_deadline the deadline that the request must be earlier than to dequeue
 * @returns 0 if successful, -ENOENT if empty or if request isn't earlier than target_deadline
 */
int
global_request_scheduler_minheap_remove_if_earlier(struct sandbox_request **removed_sandbox_request,
                                                   uint64_t                 target_deadline)
{
	assert(!software_interrupt_is_enabled());
	return priority_queue_dequeue_if_earlier(global_request_scheduler_minheap, (void **)removed_sandbox_request,
	                                         target_deadline);
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
	return priority_queue_peek(global_request_scheduler_minheap);
}

uint64_t
sandbox_request_get_priority_fn(void *element)
{
	struct sandbox_request *sandbox_request = (struct sandbox_request *)element;
	return sandbox_request->absolute_deadline;
};


/**
 * Initializes the variant and registers against the polymorphic interface
 */
void
global_request_scheduler_minheap_initialize()
{
	global_request_scheduler_minheap = priority_queue_initialize(4096, true, sandbox_request_get_priority_fn);

	struct global_request_scheduler_config config = {
		.add_fn               = global_request_scheduler_minheap_add,
		.remove_fn            = global_request_scheduler_minheap_remove,
		.remove_if_earlier_fn = global_request_scheduler_minheap_remove_if_earlier,
		.peek_fn              = global_request_scheduler_minheap_peek
	};

	global_request_scheduler_initialize(&config);
}

void
global_request_scheduler_minheap_free()
{
	priority_queue_free(global_request_scheduler_minheap);
}
