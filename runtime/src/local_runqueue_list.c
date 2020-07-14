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
	struct sandbox_request *sandbox_request;

	// If our local runqueue is empty, try to pull and allocate a sandbox request from the global request scheduler
	if (local_runqueue_is_empty()) {
		if (global_request_scheduler_remove(&sandbox_request) != GLOBAL_REQUEST_SCHEDULER_REMOVE_OK) goto err;

		struct sandbox *sandbox = sandbox_allocate(sandbox_request);
		if (!sandbox) goto sandbox_allocate_err;

		sandbox->state = SANDBOX_RUNNABLE;
		local_runqueue_add(sandbox);

	done:
		return sandbox;
	sandbox_allocate_err:
		fprintf(stderr,
		        "local_runqueue_list_get_next failed to allocate sandbox, returning request to global request "
		        "scheduler\n");
		global_request_scheduler_add(sandbox_request);
	err:
		sandbox = NULL;
		goto done;
	}

	/* Execute Round Robin Scheduling Logic */
	struct sandbox *next_sandbox = local_runqueue_list_remove_and_return();
	assert(next_sandbox->state != SANDBOX_RETURNED);
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
	debuglog("(%d,%lu) %s: run %p, %s\n", sched_getcpu(), pthread_self(), __func__, s, s->module->name);
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
