#include "local_runqueue_list.h"
#include "local_runqueue.h"
#include "global_request_scheduler.h"

__thread static struct ps_list_head local_runqueue_list;

bool
local_runqueue_list_is_empty()
{
	return ps_list_head_empty(&local_runqueue_list);
}

/* Get the sandbox at the head of the thread local runqueue */
struct sandbox *
local_runqueue_list_get_head()
{
	return ps_list_head_first_d(&local_runqueue_list, struct sandbox);
}

/**
 * Removes the thread from the thread-local runqueue
 * @param sandbox sandbox
 */
void
local_runqueue_list_remove(struct sandbox *sandbox_to_remove)
{
	ps_list_rem_d(sandbox_to_remove);
}

struct sandbox *
local_runqueue_list_remove_and_return()
{
	struct sandbox *sandbox_to_remove = ps_list_head_first_d(&local_runqueue_list, struct sandbox);
	ps_list_rem_d(sandbox_to_remove);
	return sandbox_to_remove;
}

/**
 * Execute the sandbox at the head of the thread local runqueue
 * If the runqueue is empty, pull a fresh batch of sandbox requests, instantiate them, and then execute the new head
 * @return the sandbox to execute or NULL if none are available
 */
struct sandbox *
local_runqueue_list_get_next()
{
	// If our local runqueue is empty, try to pull and allocate a sandbox request from the global request scheduler
	if (local_runqueue_is_empty()) {
		sandbox_request_t *sandbox_request;

		int return_code = global_request_scheduler_remove(&sandbox_request);
		if (return_code != 0) return NULL;

		/* TODO: sandbox_allocate should free sandbox_request on success */
		/* TODO: sandbox_allocate should return RC so we can readd sandbox_request to global_request_scheduler
		 * if needed */
		struct sandbox *sandbox = sandbox_allocate(sandbox_request);
		assert(sandbox);
		free(sandbox_request);
		sandbox->state = RUNNABLE;
		local_runqueue_add(sandbox);
		return sandbox;
	}

	/* Execute Round Robin Scheduling Logic */
	struct sandbox *next_sandbox = local_runqueue_list_remove_and_return();
	assert(next_sandbox->state != RETURNED);
	local_runqueue_add(next_sandbox);

	debuglog("[%p: %s]\n", next_sandbox, next_sandbox->module->name);
	return next_sandbox;
}


/**
 * Append a sandbox to the runqueue
 * @returns the appended sandbox
 */
void
local_runqueue_list_append(struct sandbox *sandbox_to_append)
{
	assert(ps_list_singleton_d(sandbox_to_append));
	// fprintf(stderr, "(%d,%lu) %s: run %p, %s\n", sched_getcpu(), pthread_self(), __func__, s,
	// s->module->name);
	ps_list_head_append_d(&local_runqueue_list, sandbox_to_append);
}

/**
 * Conditionally checks to see if current sandbox should be preempted FIFO doesn't preempt, so just return.
 */
void
local_runqueue_list_preempt(ucontext_t *user_context)
{
	return;
}


void
local_runqueue_list_initialize()
{
	ps_list_head_init(&local_runqueue_list);

	/* Register Function Pointers for Abstract Scheduling API */
	struct local_runqueue_config config = { .add_fn      = local_runqueue_list_append,
		                                .is_empty_fn = local_runqueue_list_is_empty,
		                                .delete_fn   = local_runqueue_list_remove,
		                                .get_next_fn = local_runqueue_list_get_next,
		                                .preempt_fn  = local_runqueue_list_preempt };
	local_runqueue_initialize(&config);
};
