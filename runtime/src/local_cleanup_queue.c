#include <threads.h>

#include "local_cleanup_queue.h"
#include "sandbox_functions.h"

/* Must be the same alignment as sandbox structs because of how the ps_list macros work */
thread_local static struct ps_list_head local_cleanup_queue PAGE_ALIGNED;
thread_local size_t local_cleanup_queue_size = 0;

void
local_cleanup_queue_initialize()
{
	ps_list_head_init(&local_cleanup_queue);
}

static inline bool
local_cleanup_queue_is_empty()
{
	return ps_list_head_empty(&local_cleanup_queue);
}

/**
 * Adds sandbox to the cleanup queue
 * @param sandbox to add to cleanup queue
 */
void
local_cleanup_queue_add(struct sandbox *sandbox)
{
	assert(sandbox);
	assert(ps_list_singleton_d(sandbox));
	ps_list_head_append_d(&local_cleanup_queue, sandbox);
	local_cleanup_queue_size++;
	assert(!local_cleanup_queue_is_empty());
}


/**
 * @brief Frees all sandboxes in the thread local cleanup queue
 * @return void
 */
void
local_cleanup_queue_free()
{
	struct sandbox *sandbox_iterator = NULL;
	struct sandbox *buffer           = NULL;
	// if (local_cleanup_queue_size > 4) debuglog("Cleanup Queue Size: %lu", local_cleanup_queue_size);

	ps_list_foreach_del_d(&local_cleanup_queue, sandbox_iterator, buffer)
	{
		assert(local_cleanup_queue_size > 0);
		ps_list_rem_d(sandbox_iterator);
		sandbox_free(sandbox_iterator);
		local_cleanup_queue_size--;
	}
}
