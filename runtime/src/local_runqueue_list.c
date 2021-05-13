#include "client_socket.h"
#include "global_request_scheduler.h"
#include "local_runqueue_list.h"
#include "local_runqueue.h"
#include "sandbox_functions.h"

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
 * Get the next sandbox and then insert at tail to "round robin"
 * @return the sandbox to execute or NULL if none are available
 */
struct sandbox *
local_runqueue_list_get_next()
{
	if (local_runqueue_list_is_empty()) return NULL;

	/* Execute Round Robin Scheduling Logic */
	struct sandbox *next_sandbox = local_runqueue_list_remove_and_return();
	assert(next_sandbox == NULL || next_sandbox->state != SANDBOX_RETURNED);
	local_runqueue_add(next_sandbox);

	return next_sandbox;
}


/**
 * Append a sandbox to the runqueue
 * @returns the appended sandbox
 */
void
local_runqueue_list_append(struct sandbox *sandbox_to_append)
{
	assert(sandbox_to_append != NULL);
	assert(ps_list_singleton_d(sandbox_to_append));
	ps_list_head_append_d(&local_runqueue_list, sandbox_to_append);
}

void
local_runqueue_list_initialize()
{
	ps_list_head_init(&local_runqueue_list);

	/* Register Function Pointers for Abstract Scheduling API */
	struct local_runqueue_config config = { .add_fn      = local_runqueue_list_append,
		                                .is_empty_fn = local_runqueue_list_is_empty,
		                                .delete_fn   = local_runqueue_list_remove,
		                                .get_next_fn = local_runqueue_list_get_next };
	local_runqueue_initialize(&config);
};
