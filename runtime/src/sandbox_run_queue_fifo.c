#include "sandbox_run_queue_fifo.h"
#include "sandbox_run_queue.h"
#include "sandbox_request_scheduler.h"

__thread static struct ps_list_head sandbox_run_queue_fifo;

bool
sandbox_run_queue_fifo_is_empty()
{
	return ps_list_head_empty(&sandbox_run_queue_fifo);
}

// Get the sandbox at the head of the thread local runqueue
struct sandbox *
sandbox_run_queue_fifo_get_head()
{
	return ps_list_head_first_d(&sandbox_run_queue_fifo, struct sandbox);
}

/**
 * Removes the thread from the thread-local runqueue
 * @param sandbox sandbox
 **/
void
sandbox_run_queue_fifo_remove(struct sandbox *sandbox_to_remove)
{
	ps_list_rem_d(sandbox_to_remove);
}

struct sandbox *
sandbox_run_queue_fifo_remove_and_return()
{
	struct sandbox *sandbox_to_remove = ps_list_head_first_d(&sandbox_run_queue_fifo, struct sandbox);
	ps_list_rem_d(sandbox_to_remove);
	return sandbox_to_remove;
}

/**
 * Execute the sandbox at the head of the thread local runqueue
 * If the runqueue is empty, pull a fresh batch of sandbox requests, instantiate them, and then execute the new head
 * @return the sandbox to execute or NULL if none are available
 **/
struct sandbox *
sandbox_run_queue_fifo_get_next()
{
	if (sandbox_run_queue_is_empty()) {
		sandbox_request_t *sandbox_request = sandbox_request_scheduler_remove();
		if (sandbox_request == NULL) return NULL;
		struct sandbox *sandbox = sandbox_allocate(sandbox_request);
		assert(sandbox);
		free(sandbox_request);
		sandbox->state = RUNNABLE;
		sandbox_run_queue_add(sandbox);
		return sandbox;
	}

	// Execute Round Robin Scheduling Logic
	struct sandbox *next_sandbox = sandbox_run_queue_fifo_remove_and_return();
	assert(next_sandbox->state != RETURNED);
	sandbox_run_queue_add(next_sandbox);

	debuglog("[%p: %s]\n", next_sandbox, next_sandbox->module->name);
	return next_sandbox;
}


// Append a sandbox to the runqueue
struct sandbox *
sandbox_run_queue_fifo_append(struct sandbox *sandbox_to_append)
{
	assert(ps_list_singleton_d(sandbox_to_append));
	// fprintf(stderr, "(%d,%lu) %s: run %p, %s\n", sched_getcpu(), pthread_self(), __func__, s,
	// s->module->name);
	ps_list_head_append_d(&sandbox_run_queue_fifo, sandbox_to_append);
	return sandbox_to_append;
}

// Conditionally checks to see if current sandbox should be preempted
// FIFO doesn't preempt, so just return.
void
sandbox_run_queue_fifo_preempt(ucontext_t *user_context)
{
	return;
}


void
sandbox_run_queue_fifo_initialize()
{
	ps_list_head_init(&sandbox_run_queue_fifo);

	// Register Function Pointers for Abstract Scheduling API
	sandbox_run_queue_config_t config = { .add      = sandbox_run_queue_fifo_append,
		                              .is_empty = sandbox_run_queue_fifo_is_empty,
		                              .delete   = sandbox_run_queue_fifo_remove,
		                              .get_next = sandbox_run_queue_fifo_get_next,
		                              .preempt  = sandbox_run_queue_fifo_preempt };
	sandbox_run_queue_initialize(&config);
};
