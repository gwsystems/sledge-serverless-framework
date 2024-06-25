#include <stdnoreturn.h>

#include "global_request_scheduler.h"
#include "panic.h"

static struct sandbox_metadata global_highest_priority_metadata;

/* Default uninitialized implementations of the polymorphic interface */
noreturn static struct sandbox *
uninitialized_add(struct sandbox *arg)
{
	panic("Global Request Scheduler Add was called before initialization\n");
}

noreturn static int
uninitialized_remove(struct sandbox **arg)
{
	panic("Global Request Scheduler Remove was called before initialization\n");
}

noreturn static uint64_t
uninitialized_peek()
{
	panic("Global Request Scheduler Peek was called before initialization\n");
}


/* The global of our polymorphic interface */
static struct global_request_scheduler_config global_request_scheduler = {.add_fn    = uninitialized_add,
                                                                          .remove_fn = uninitialized_remove,
                                                                          .peek_fn   = uninitialized_peek};

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
 * Adds a sandbox to the request scheduler
 * @param sandbox
 * @returns pointer to sandbox if added. NULL otherwise
 */
struct sandbox *
global_request_scheduler_add(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	return global_request_scheduler.add_fn(sandbox);
}

/**
 * Removes a sandbox according to the scheduling policy of the variant
 * @param removed_sandbox where to write the adddress of the removed sandbox
 * @returns 0 if successfully returned a sandbox, -ENOENT if empty, -EAGAIN if atomic operation unsuccessful
 */
int
global_request_scheduler_remove(struct sandbox **removed_sandbox)
{
	assert(removed_sandbox != NULL);
	return global_request_scheduler.remove_fn(removed_sandbox);
}

/**
 * Removes a sandbox according to the scheduling policy of the variant
 * @param removed_sandbox where to write the adddress of the removed sandbox
 * @param target_deadline the deadline that must be validated before dequeuing
 * @returns 0 if successfully returned a sandbox, -ENOENT if empty or if no element meets target_deadline,
 * -EAGAIN if atomic operation unsuccessful
 */
int
global_request_scheduler_remove_if_earlier(struct sandbox **removed_sandbox, uint64_t target_deadline)
{
	assert(removed_sandbox != NULL);
	return global_request_scheduler.remove_if_earlier_fn(removed_sandbox, target_deadline);
}

/**
 * Peeks at the priority of the highest priority sandbox
 * @returns highest priority
 */
uint64_t
global_request_scheduler_peek()
{
	return global_request_scheduler.peek_fn();
}

/**
 * Peeks at the metadata of the highest priority sandbox
 * @returns metadata of the highest priority sandbox
 */
struct sandbox_metadata
global_request_scheduler_peek_metadata()
{
	return global_highest_priority_metadata;
}

/**
 * Updates the metadata of the highest priority sandbox
 * @param element the highest priority sandbox
 */
void
global_request_scheduler_update_highest_priority(const void *element)
{
	if (element == NULL) {
		global_highest_priority_metadata.absolute_deadline    = UINT64_MAX;
		global_highest_priority_metadata.tenant               = NULL;
		global_highest_priority_metadata.route                = NULL;
		global_highest_priority_metadata.allocation_timestamp = 0;
		global_highest_priority_metadata.remaining_exec  = 0;
		global_highest_priority_metadata.id                   = 0;
		global_highest_priority_metadata.global_queue_type    = 0;
		global_highest_priority_metadata.exceeded_estimation  = false;
		global_highest_priority_metadata.state                = SANDBOX_UNINITIALIZED;
		return;
	}

	const struct sandbox *sandbox                         = element;
	global_highest_priority_metadata.absolute_deadline    = sandbox->absolute_deadline;
	global_highest_priority_metadata.tenant               = sandbox->tenant;
	global_highest_priority_metadata.route                = sandbox->route;
	global_highest_priority_metadata.allocation_timestamp = sandbox->timestamp_of.allocation;
	global_highest_priority_metadata.remaining_exec  = sandbox->remaining_exec;
	// global_highest_priority_metadata.remaining_exec  = sandbox->remaining_execution_original;
	global_highest_priority_metadata.id                   = sandbox->id;
	global_highest_priority_metadata.exceeded_estimation  = sandbox->exceeded_estimation;
	global_highest_priority_metadata.state                = sandbox->state;
}
