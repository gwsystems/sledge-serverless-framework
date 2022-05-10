#include <threads.h>

#include "local_completion_queue.h"
#include "sandbox_functions.h"

/* Must be the same alignment as sandbox structs because of how the ps_list macros work */
thread_local static struct ps_list_head local_completion_queue PAGE_ALIGNED;


void
local_completion_queue_initialize()
{
	ps_list_head_init(&local_completion_queue);
}

static inline bool
local_completion_queue_is_empty()
{
	return ps_list_head_empty(&local_completion_queue);
}

/**
 * Adds sandbox to the completion queue
 * @param sandbox to add to completion queue
 */
void
local_completion_queue_add(struct sandbox *sandbox)
{
	assert(sandbox);
	assert(ps_list_singleton_d(sandbox));
	ps_list_head_append_d(&local_completion_queue, sandbox);
	assert(!local_completion_queue_is_empty());
}


/**
 * @brief Frees all sandboxes in the thread local completion queue
 * @return void
 */
void
local_completion_queue_free()
{
	struct sandbox *sandbox_iterator = NULL;
	struct sandbox *buffer           = NULL;

	ps_list_foreach_del_d(&local_completion_queue, sandbox_iterator, buffer)
	{
		ps_list_rem_d(sandbox_iterator);
		sandbox_free(sandbox_iterator);
	}
}
