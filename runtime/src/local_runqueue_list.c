#include <threads.h>

#include "current_sandbox.h"
#include "global_request_scheduler.h"
#include "local_runqueue_list.h"
#include "local_runqueue.h"
#include "sandbox_functions.h"

extern thread_local int global_worker_thread_idx;
extern _Atomic uint32_t local_queue_length[1024];
extern uint32_t max_local_queue_length[1024];
extern struct ps_list_head * worker_fifo_queue[1024];
thread_local static struct ps_list_head local_runqueue_list;
uint32_t runtime_fifo_queue_batch_size = 1;

int 
local_runqueue_list_get_length_index(int index)
{
	return local_queue_length[index];	
}

int
local_runqueue_list_get_length()
{
	return local_queue_length[global_worker_thread_idx];
}

bool
local_runqueue_list_is_empty()
{
	return ps_list_head_empty(&local_runqueue_list);
}

bool
local_runqueue_list_is_empty_index(int index)
{
	struct ps_list_head *local_queue = worker_fifo_queue[index];
	assert(local_queue != NULL);
	return ps_list_head_empty(local_queue);
}

/* Get the sandbox at the head of the thread local runqueue */
struct sandbox *
local_runqueue_list_get_head()
{
	return ps_list_head_first_d(&local_runqueue_list, struct sandbox);
}

/**
 * Removes the sandbox from the thread-local runqueue
 * @param sandbox sandbox
 */
void
local_runqueue_list_remove_nolock(struct sandbox *sandbox_to_remove)
{
	ps_list_rem_d(sandbox_to_remove);
	atomic_fetch_sub(&local_queue_length[global_worker_thread_idx], 1);
}

/**
 * Removes the sandbox from the thread-local runqueue
 * @param sandbox sandbox
 */
void
local_runqueue_list_remove(struct sandbox *sandbox_to_remove)
{
	lock_node_t node = {};
	lock_lock(&local_runqueue_list.lock, &node);
	ps_list_rem_d(sandbox_to_remove);
	lock_unlock(&local_runqueue_list.lock, &node);
	atomic_fetch_sub(&local_queue_length[global_worker_thread_idx], 1);
}

struct sandbox *
local_runqueue_list_remove_and_return()
{
	struct sandbox *sandbox_to_remove = ps_list_head_first_d(&local_runqueue_list, struct sandbox);
	ps_list_rem_d(sandbox_to_remove);
	atomic_fetch_sub(&local_queue_length[global_worker_thread_idx], 1);
	return sandbox_to_remove;
}

/**
 * Append a sandbox to the tail of the runqueue, only called by the self thread
 * Only called by the self thread and pull requests from the GQ, no need lock
 * @returns the appended sandbox
 */
void
local_runqueue_list_append(struct sandbox *sandbox_to_append)
{
	assert(sandbox_to_append != NULL);
	assert(ps_list_singleton_d(sandbox_to_append));
	ps_list_head_append_d(&local_runqueue_list, sandbox_to_append);
	atomic_fetch_add(&local_queue_length[global_worker_thread_idx], 1);
	if (local_queue_length[global_worker_thread_idx] > max_local_queue_length[global_worker_thread_idx]) {
                max_local_queue_length[global_worker_thread_idx] = local_queue_length[global_worker_thread_idx];
        }
}

/**
 * Append a sandbox to the tail of the runqueue, will be called by the listener thread, need lock
 * @returns the appended sandbox
 */
void
local_runqueue_list_append_index(int index, struct sandbox *sandbox_to_append)
{
	struct ps_list_head *local_queue = worker_fifo_queue[index];
	assert(local_queue != NULL);
	assert(sandbox_to_append != NULL);
	assert(ps_list_singleton_d(sandbox_to_append));
	lock_node_t node = {};
	lock_lock(&local_queue->lock, &node);
	ps_list_head_append_d(local_queue, sandbox_to_append);
	lock_unlock(&local_queue->lock, &node);
	atomic_fetch_add(&local_queue_length[index], 1);
	if (local_queue_length[index] > max_local_queue_length[index]) {
		max_local_queue_length[index] = local_queue_length[index];
	}
}

/* Remove sandbox from head of runqueue and add it to tail */
void
local_runqueue_list_rotate()
{
	if (dispatcher == DISPATCHER_LLD) {
		/* need lock */
		lock_node_t node = {};
        	lock_lock(&local_runqueue_list.lock, &node);
		/* If runqueue is size one, skip round robin logic since tail equals head */
                if (ps_list_head_one_node(&local_runqueue_list)) {
			lock_unlock(&local_runqueue_list.lock, &node);
			return;
		}

                struct sandbox *sandbox_at_head = local_runqueue_list_remove_and_return();
                assert(sandbox_at_head->state == SANDBOX_INTERRUPTED);
                local_runqueue_list_append(sandbox_at_head);
		lock_unlock(&local_runqueue_list.lock, &node);
	} else { /* no need lock */
		/* If runqueue is size one, skip round robin logic since tail equals head */
		if (ps_list_head_one_node(&local_runqueue_list)) return;

		struct sandbox *sandbox_at_head = local_runqueue_list_remove_and_return();
		assert(sandbox_at_head->state == SANDBOX_INTERRUPTED);
		local_runqueue_list_append(sandbox_at_head);
	}
}

/**
 * Get the next sandbox
 * @return the sandbox to execute or NULL if none are available
 */
struct sandbox *
local_runqueue_list_get_next_nolock()
{
	if (local_runqueue_list_is_empty()) return NULL;

	return local_runqueue_list_get_head();
}

struct sandbox *
local_runqueue_list_get_next()
{
        if (local_runqueue_list_is_empty()) return NULL;
	lock_node_t node = {};
	lock_lock(&local_runqueue_list.lock, &node);
        struct sandbox *sandbox = local_runqueue_list_get_head();
	lock_unlock(&local_runqueue_list.lock, &node);
	return sandbox;
}

void
local_runqueue_list_initialize()
{
	ps_list_head_init(&local_runqueue_list);
	worker_fifo_queue[global_worker_thread_idx] = &local_runqueue_list;

	/* Register Function Pointers for Abstract Scheduling API */
	struct local_runqueue_config config = { .add_fn            = local_runqueue_list_append,
						.add_fn_idx        = local_runqueue_list_append_index,
						.get_length_fn_idx = local_runqueue_list_get_length_index,
						.get_length_fn     = local_runqueue_list_get_length,
		                                .is_empty_fn       = local_runqueue_list_is_empty,
						.is_empty_fn_idx   = local_runqueue_list_is_empty_index};
	if (dispatcher == DISPATCHER_LLD) {
		config.delete_fn = local_runqueue_list_remove;
		config.get_next_fn = local_runqueue_list_get_next;
	} else {// must be DISPATCHER_TO_GLOBAL_QUEUE 
		config.delete_fn = local_runqueue_list_remove_nolock;
		config.get_next_fn = local_runqueue_list_get_next_nolock;
	}
	local_runqueue_initialize(&config);
};
