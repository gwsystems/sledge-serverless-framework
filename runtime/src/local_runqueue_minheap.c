#include <stdint.h>
#include <threads.h>

#include "arch/context.h"
#include "client_socket.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "global_request_scheduler.h"
#include "local_runqueue.h"
#include "local_runqueue_minheap.h"
#include "panic.h"
#include "priority_queue.h"
#include "sandbox_functions.h"
#include "runtime.h"

thread_local static struct priority_queue *local_runqueue_minheap;

/**
 * Checks if the run queue is empty
 * @returns true if empty. false otherwise
 */
bool
local_runqueue_minheap_is_empty()
{
	return priority_queue_length_nolock(local_runqueue_minheap) == 0;
}

/**
 * Adds a sandbox to the run queue
 * @param sandbox
 * @returns pointer to sandbox added
 */
void
local_runqueue_minheap_add(struct sandbox *sandbox)
{
	int return_code = priority_queue_enqueue_nolock(local_runqueue_minheap, sandbox);
	/* TODO: propagate RC to caller. Issue #92 */
	if (return_code == -ENOSPC) panic("Thread Runqueue is full!\n");
}

/**
 * Deletes a sandbox from the runqueue
 * @param sandbox to delete
 */
static void
local_runqueue_minheap_delete(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	int rc = priority_queue_delete_nolock(local_runqueue_minheap, sandbox);
	if (rc == -1) panic("Tried to delete sandbox %lu from runqueue, but was not present\n", sandbox->id);
}

/**
 * This function determines the next sandbox to run.
 * This is the head of the runqueue
 *
 * Execute the sandbox at the head of the thread local runqueue
 * @return the sandbox to execute or NULL if none are available
 */
struct sandbox *
local_runqueue_minheap_get_next()
{
	/* Get the deadline of the sandbox at the head of the local request queue */
	struct sandbox *next = NULL;
	int             rc   = priority_queue_top_nolock(local_runqueue_minheap, (void **)&next);

	if (rc == -ENOENT) return NULL;

	return next;
}

/**
 * Registers the PS variant with the polymorphic interface
 */
void
local_runqueue_minheap_initialize()
{
	/* Initialize local state */
	local_runqueue_minheap = priority_queue_initialize(256, false, sandbox_get_priority);

	/* Register Function Pointers for Abstract Scheduling API */
	struct local_runqueue_config config = { .add_fn      = local_runqueue_minheap_add,
		                                .is_empty_fn = local_runqueue_minheap_is_empty,
		                                .delete_fn   = local_runqueue_minheap_delete,
		                                .get_next_fn = local_runqueue_minheap_get_next };

	local_runqueue_initialize(&config);
}
