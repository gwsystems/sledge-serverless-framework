#include "sandbox_run_queue_fifo.h"
#include "sandbox_run_queue.h"

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

struct sandbox *
sandbox_run_queue_fifo_remove_and_return()
{
	struct sandbox *sandbox_to_remove = ps_list_head_first_d(&sandbox_run_queue_fifo, struct sandbox);
	ps_list_rem_d(sandbox_to_remove);
	return sandbox_to_remove;
}


void
sandbox_run_queue_fifo_initialize()
{
	ps_list_head_init(&sandbox_run_queue_fifo);

	// Register Function Pointers for Abstract Scheduling API
	sandbox_run_queue_config_t config = { .add      = sandbox_run_queue_fifo_append,
		                              .is_empty = sandbox_run_queue_fifo_is_empty,
		                              .remove   = sandbox_run_queue_fifo_remove_and_return,
		                              .delete   = sandbox_run_queue_fifo_remove };

	sandbox_run_queue_initialize(&config);
}