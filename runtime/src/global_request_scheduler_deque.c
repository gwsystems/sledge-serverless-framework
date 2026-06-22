#include "global_request_scheduler_deque.h"
#include "global_request_scheduler.h"
#include "runtime.h"

#define GLOBAL_REQUEST_SCHEDULER_DEQUE_CAPACITY (1 << 12)

static struct deque_sandbox *global_request_scheduler_deque;

/**
 * Pushes a sandbox to the global deque.
 *
 * No lock is required: this is a work-stealing deque used as a single-producer / multi-consumer queue. Only the
 * listener thread ever pushes (the "owner" end), while worker threads only ever steal (the lock-free opposite
 * end, coordinated by a CAS). The owner's other operation, deque_pop, is never used, so the multi-owner case the
 * deque comments warn about does not arise here.
 *
 * @param sandbox
 * @returns pointer to sandbox if added. NULL otherwise
 */
static struct sandbox *
global_request_scheduler_deque_add(struct sandbox *sandbox)
{
	int return_code = 1;

	return_code = deque_push_sandbox(global_request_scheduler_deque, &sandbox);

	if (return_code != 0) return NULL;
	return sandbox;
}

/**
 * Stealing from the dequeue is a lock-free, cross-core "pop", which removes the element from the end opposite to
 * "pop". Because the producer and consumer (the core stealine the sandbox) modify different ends,
 * no locks are required, and coordination is achieved by instead retrying on inconsistent indices.
 *
 * Relevant Read: https://www.dre.vanderbilt.edu/~schmidt/PDF/work-stealing-dequeue.pdf
 *
 * @returns 0 if successfully returned a sandbox, -ENOENT if empty, -EAGAIN if atomic instruction unsuccessful
 */
static int
global_request_scheduler_deque_remove(struct sandbox **removed_sandbox)
{
	return deque_steal_sandbox(global_request_scheduler_deque, removed_sandbox);
}

static int
global_request_scheduler_deque_remove_if_earlier(struct sandbox **removed_sandbox, uint64_t target_deadline)
{
	panic("Deque variant does not support this call\n");
	return -1;
}

void
global_request_scheduler_deque_initialize()
{
	/* Allocate and Initialize the global deque */
	global_request_scheduler_deque = (struct deque_sandbox *)calloc(1, sizeof(struct deque_sandbox));
	assert(global_request_scheduler_deque);
	/* Note: Below is a Macro. It heap-allocates the backing buffer sized to the requested capacity. */
	int rc = deque_init_sandbox(global_request_scheduler_deque, GLOBAL_REQUEST_SCHEDULER_DEQUE_CAPACITY);
	if (rc != 0) panic("Failed to allocate global request scheduler deque\n");

	/* Register Function Pointers for Abstract Scheduling API */
	struct global_request_scheduler_config config = {.add_fn    = global_request_scheduler_deque_add,
	                                                 .remove_fn = global_request_scheduler_deque_remove,
	                                                 .remove_if_earlier_fn =
	                                                   global_request_scheduler_deque_remove_if_earlier};

	global_request_scheduler_initialize(&config);
}
