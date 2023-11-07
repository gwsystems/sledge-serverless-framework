
#ifdef LOG_LOCAL_RUNQUEUE
#include <stdint.h>
#endif
#include <threads.h>

#include "local_runqueue.h"

extern _Atomic uint64_t worker_queuing_cost[1024];
thread_local uint32_t total_local_requests = 0;
static struct local_runqueue_config local_runqueue;

#ifdef LOG_LOCAL_RUNQUEUE
thread_local uint32_t local_runqueue_count = 0;
#endif

/* Initializes a concrete implementation of the sandbox request scheduler interface */
void
local_runqueue_initialize(struct local_runqueue_config *config)
{
	memcpy(&local_runqueue, config, sizeof(struct local_runqueue_config));
}

/**
 * Adds a sandbox to the run queue
 * @param sandbox to add
 */
void
local_runqueue_add(struct sandbox *sandbox)
{
	assert(local_runqueue.add_fn != NULL);
#ifdef LOG_LOCAL_RUNQUEUE
	local_runqueue_count++;
#endif
	return local_runqueue.add_fn(sandbox);
}

void
local_runqueue_add_index(int index, struct sandbox *sandbox)
{
        assert(local_runqueue.add_fn_idx != NULL);
#ifdef LOG_LOCAL_RUNQUEUE
        local_runqueue_count++;
#endif
        return local_runqueue.add_fn_idx(index, sandbox);
}

uint64_t
local_runqueue_try_add_index(int index, struct sandbox *sandbox, bool *need_interrupt)
{
	assert(local_runqueue.try_add_fn_idx != NULL);
	return local_runqueue.try_add_fn_idx(index, sandbox, need_interrupt);
}
/**
 * Delete a sandbox from the run queue
 * @param sandbox to delete
 */
void
local_runqueue_delete(struct sandbox *sandbox)
{
	assert(local_runqueue.delete_fn != NULL);
	total_local_requests++;
#ifdef LOG_LOCAL_RUNQUEUE
	local_runqueue_count--;
#endif
	local_runqueue.delete_fn(sandbox);
}

/**
 * Checks if run queue is empty
 * @returns true if empty
 */
bool
local_runqueue_is_empty()
{
	assert(local_runqueue.is_empty_fn != NULL);
	return local_runqueue.is_empty_fn();
}

/**
 * Get height if run queue is a binary search tree
 */

int local_runqueue_get_height() {
	return local_runqueue.get_height_fn();
}

/**
 * Get total count of items in the queue 
 */
int local_runqueue_get_length() {
	return local_runqueue.get_length_fn();
}
int local_runqueue_get_length_index(int index) {
	return local_runqueue.get_length_fn_idx(index);
}
/**
 * Get next sandbox from run queue, where next is defined by
 * @returns sandbox (or NULL?)
 */
struct sandbox *
local_runqueue_get_next()
{
	assert(local_runqueue.get_next_fn != NULL);
	return local_runqueue.get_next_fn();
};

void
worker_queuing_cost_initialize()
{
        for (int i = 0; i < 1024; i++) atomic_init(&worker_queuing_cost[i], 0);
}

void
worker_queuing_cost_increment(int index, uint64_t cost)
{
        atomic_fetch_add(&worker_queuing_cost[index], cost);
}

void
worker_queuing_cost_decrement(int index, uint64_t cost)
{
        assert(index >= 0 && index < 1024);
        atomic_fetch_sub(&worker_queuing_cost[index], cost);
        assert(worker_queuing_cost[index] >= 0);
}

