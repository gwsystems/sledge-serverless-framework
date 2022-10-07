#include <stdint.h>
#include <threads.h>

#include "arch/context.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "global_request_scheduler.h"
#include "local_runqueue.h"
#include "local_runqueue_minheap.h"
#include "panic.h"
#include "priority_queue.h"
#include "sandbox_functions.h"
#include "runtime.h"

extern struct priority_queue* worker_queues[1024];
extern thread_local int worker_thread_idx;
thread_local static struct priority_queue *local_runqueue_minheap;

/**
 * Checks if the run queue is empty
 * @returns true if empty. false otherwise
 */
bool
local_runqueue_minheap_is_empty()
{
	//return priority_queue_length_nolock(local_runqueue_minheap) == 0;
	return priority_queue_length(local_runqueue_minheap) == 0;
}

/**
 * Adds a sandbox to the run queue
 * @param sandbox
 * @returns pointer to sandbox added
 */
void
local_runqueue_minheap_add(struct sandbox *sandbox)
{
	/*int return_code = priority_queue_enqueue_nolock(local_runqueue_minheap, sandbox);
	if (unlikely(return_code == -ENOSPC)) {
		struct priority_queue *temp = priority_queue_grow_nolock(local_runqueue_minheap);
		if (unlikely(temp == NULL)) panic("Failed to grow local runqueue\n");
		local_runqueue_minheap = temp;
		return_code            = priority_queue_enqueue_nolock(local_runqueue_minheap, sandbox);
		if (unlikely(return_code == -ENOSPC)) panic("Thread Runqueue is full!\n");
	}*/

	int return_code = priority_queue_enqueue(local_runqueue_minheap, sandbox);
	if (return_code != 0) {
		printf("add request to local queue failed, exit\n");
		exit(0);
	}
}

void
local_runqueue_minheap_add_index(int index, struct sandbox *sandbox)
{
	int return_code = priority_queue_enqueue(worker_queues[index], sandbox);
        if (return_code != 0) {
                printf("add request to local queue failed, exit\n");
                exit(0);
        } 
}

/**
 * Deletes a sandbox from the runqueue
 * @param sandbox to delete
 */
static void
local_runqueue_minheap_delete(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	//int rc = priority_queue_delete_nolock(local_runqueue_minheap, sandbox);
	int rc = priority_queue_delete(local_runqueue_minheap, sandbox);
	if (rc == -1) panic("Tried to delete sandbox %lu from runqueue, but was not present\n", sandbox->id);
}

/**
 * This function determines the next sandbox to run.
 * This is the head of the local runqueue
 *
 * Execute the sandbox at the head of the thread local runqueue
 * @return the sandbox to execute or NULL if none are available
 */
struct sandbox *
local_runqueue_minheap_get_next()
{
	/* Get the deadline of the sandbox at the head of the local request queue */
	struct sandbox *next = NULL;
	//int             rc   = priority_queue_top_nolock(local_runqueue_minheap, (void **)&next);
	int             rc   = priority_queue_top(local_runqueue_minheap, (void **)&next);

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
	local_runqueue_minheap = priority_queue_initialize(RUNTIME_RUNQUEUE_SIZE, true, sandbox_get_priority);

	worker_queues[worker_thread_idx] = local_runqueue_minheap;
	/* Register Function Pointers for Abstract Scheduling API */
	struct local_runqueue_config config = { .add_fn      = local_runqueue_minheap_add,
						.add_fn_idx  = local_runqueue_minheap_add_index,
		                                .is_empty_fn = local_runqueue_minheap_is_empty,
		                                .delete_fn   = local_runqueue_minheap_delete,
		                                .get_next_fn = local_runqueue_minheap_get_next };

	local_runqueue_initialize(&config);
}
