#include "global_request_scheduler.h"
#include "panic.h"

/* Default uninitialized implementations of the polymorphic interface */
__attribute__((noreturn)) static struct sandbox_request *
uninitialized_add(void *arg)
{
	panic("Global Request Scheduler Add was called before initialization\n");
};

__attribute__((noreturn)) static int
uninitialized_remove(struct sandbox_request **arg)
{
	panic("Global Request Scheduler Remove was called before initialization\n");
};

__attribute__((noreturn)) static uint64_t
uninitialized_peek()
{
	panic("Global Request Scheduler Peek was called before initialization\n");
};


/* The global of our polymorphic interface */
static struct global_request_scheduler_config global_request_scheduler = { .add_fn    = uninitialized_add,
	                                                                   .remove_fn = uninitialized_remove,
	                                                                   .peek_fn   = uninitialized_peek };

/**
 * Initializes the polymorphic interface with a concrete implementation
 * @param config
 */
void
global_request_scheduler_initialize(struct global_request_scheduler_config *config)
{
	assert(config != NULL);
	memcpy(&global_request_scheduler, config, sizeof(struct global_request_scheduler_config));
}


/**
 * Adds a sandbox request to the request scheduler
 * @param sandbox_request
 */
struct sandbox_request *
global_request_scheduler_add(struct sandbox_request *sandbox_request)
{
	assert(sandbox_request != NULL);
	return global_request_scheduler.add_fn(sandbox_request);
}

/**
 * Removes a sandbox request according to the scheduling policy of the variant
 * @param removed_sandbox where to write the adddress of the removed sandbox
 * @returns 0 if successfully returned a sandbox request, -ENOENT if empty, -EAGAIN if atomic operation unsuccessful
 */
int
global_request_scheduler_remove(struct sandbox_request **removed_sandbox)
{
	assert(removed_sandbox != NULL);
	return global_request_scheduler.remove_fn(removed_sandbox);
}

/**
 * Peeks at the priority of the highest priority sandbox request
 * @returns highest priority
 */
uint64_t
global_request_scheduler_peek()
{
	return global_request_scheduler.peek_fn();
};
