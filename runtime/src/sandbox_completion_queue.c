#include "sandbox_completion_queue.h"

__thread static struct ps_list_head sandbox_completion_queue;


void
sandbox_completion_queue_initialize()
{
	ps_list_head_init(&sandbox_completion_queue);
}

static inline bool
sandbox_completion_queue_is_empty()
{
	return ps_list_head_empty(&sandbox_completion_queue);
}

/**
 * Adds sandbox to the completion queue
 * @param sandbox
 **/
void
sandbox_completion_queue_add(struct sandbox *sandbox)
{
	assert(sandbox);
	assert(ps_list_singleton_d(sandbox));
	ps_list_head_append_d(&sandbox_completion_queue, sandbox);
	assert(!sandbox_completion_queue_is_empty());
}


/**
 * @brief Frees all sandboxes in the thread local completion queue
 * @return void
 */
void
sandbox_completion_queue_free()
{
	struct sandbox *sandbox_iterator;
	struct sandbox *buffer;

	ps_list_foreach_del_d(&sandbox_completion_queue, sandbox_iterator, buffer)
	{
		ps_list_rem_d(sandbox_iterator);
		sandbox_free(sandbox_iterator);
	}
}