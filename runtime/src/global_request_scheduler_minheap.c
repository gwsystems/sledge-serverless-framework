#include <assert.h>
#include <errno.h>

#include "global_request_scheduler.h"
#include "listener_thread.h"
#include "panic.h"
#include "priority_queue.h"
#include "runtime.h"

static struct priority_queue *global_request_scheduler_minheap;

/**
 * Pushes a sandbox to the global deque
 * @param sandbox
 * @returns pointer to request if added. Panics runtime otherwise
 */
static struct sandbox *
global_request_scheduler_minheap_add(void *sandbox_raw)
{
	assert(sandbox_raw);
	assert(global_request_scheduler_minheap);
	if (unlikely(!listener_thread_is_running())) panic("%s is only callable by the listener thread\n", __func__);

	int return_code = priority_queue_enqueue(global_request_scheduler_minheap, sandbox_raw);
	/* TODO: Propagate -1 to caller. Issue #91 */
	if (return_code == -ENOSPC) panic("Request Queue is full\n");
	return (struct sandbox *)sandbox_raw;
}

/**
 * @param pointer to the pointer that we want to set to the address of the removed sandbox
 * @returns 0 if successful, -ENOENT if empty
 */
int
global_request_scheduler_minheap_remove(struct sandbox **removed_sandbox)
{
	return priority_queue_dequeue(global_request_scheduler_minheap, (void **)removed_sandbox);
}

/**
 * @param removed_sandbox pointer to set to removed sandbox
 * @param target_deadline the deadline that the request must be earlier than to dequeue
 * @returns 0 if successful, -ENOENT if empty or if request isn't earlier than target_deadline
 */
int
global_request_scheduler_minheap_remove_if_earlier(struct sandbox **removed_sandbox, uint64_t target_deadline)
{
	return priority_queue_dequeue_if_earlier(global_request_scheduler_minheap, (void **)removed_sandbox,
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
sandbox_get_priority_fn(void *element)
{
	struct sandbox *sandbox = (struct sandbox *)element;
	return sandbox->absolute_deadline;
};


/**
 * Initializes the variant and registers against the polymorphic interface
 */
void
global_request_scheduler_minheap_initialize()
{
	global_request_scheduler_minheap = priority_queue_initialize(4096, true, sandbox_get_priority_fn);

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
