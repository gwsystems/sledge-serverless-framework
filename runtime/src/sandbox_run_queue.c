#include "sandbox_run_queue.h"

__thread static struct ps_list_head sandbox_run_queue;

void
sandbox_run_queue_initialize()
{
	ps_list_head_init(&sandbox_run_queue);
}

bool
sandbox_run_queue_is_empty()
{
	return ps_list_head_empty(&sandbox_run_queue);
}

// Get the sandbox at the head of the thread local runqueue
struct sandbox *
sandbox_run_queue_get_head()
{
	return ps_list_head_first_d(&sandbox_run_queue, struct sandbox);
}

// Remove a sandbox from the runqueue
void
sandbox_run_queue_remove(struct sandbox *sandbox_to_remove)
{
	ps_list_rem_d(sandbox_to_remove);
}

// Append a sandbox to the runqueue
void
sandbox_run_queue_append(struct sandbox *sandbox_to_append)
{
	assert(ps_list_singleton_d(sandbox_to_append));
	// fprintf(stderr, "(%d,%lu) %s: run %p, %s\n", sched_getcpu(), pthread_self(), __func__, s,
	// s->module->name);
	ps_list_head_append_d(&sandbox_run_queue, sandbox_to_append);
}