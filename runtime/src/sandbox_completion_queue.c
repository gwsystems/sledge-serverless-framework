#include "sandbox_completion_queue.h"

__thread static struct ps_list_head sandbox_completion_queue;


void
sandbox_completion_queue_initialize()
{
	ps_list_head_init(&sandbox_completion_queue);
}

/**
 * Adds sandbox to the completion queue
 * @param sandbox
 **/
void
sandbox_completion_queue_add(struct sandbox *sandbox)
{
	assert(ps_list_singleton_d(sandbox));
	ps_list_head_append_d(&sandbox_completion_queue, sandbox);
}

static inline bool
sandbox_completion_queue_is_empty()
{
	return ps_list_head_empty(&sandbox_completion_queue);
}

/**
 * @brief Pops n sandboxes from the thread local completion queue and then frees them
 * @param number_to_free The number of sandboxes to pop and free
 * @return void
 */
void
sandbox_completion_queue_free(unsigned int number_to_free)
{
	for (int i = 0; i < number_to_free; i++) {
		if (sandbox_completion_queue_is_empty()) break;
		struct sandbox *sandbox = ps_list_head_first_d(&sandbox_completion_queue, struct sandbox);
		if (!sandbox) break;
		ps_list_rem_d(sandbox);
		sandbox_free(sandbox);
	}
}