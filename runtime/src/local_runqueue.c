
#ifdef LOG_LOCAL_RUNQUEUE
#include <stdint.h>
#endif
#include <threads.h>

#include "local_runqueue.h"

extern thread_local int global_worker_thread_idx;
extern bool runtime_worker_busy_loop_enabled;
extern pthread_mutex_t mutexs[1024];
extern pthread_cond_t conds[1024];
extern sem_t semlock[1024]; 

extern _Atomic uint64_t worker_queuing_cost[1024];
static struct local_runqueue_config local_runqueue;
thread_local uint32_t total_complete_requests = 0;
_Atomic uint32_t local_runqueue_count[1024];
struct timespec startT[1024];

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
	local_runqueue.add_fn(sandbox);
	//atomic_fetch_add(&local_runqueue_count[global_worker_thread_idx], 1);
}

void
local_runqueue_add_index(int index, struct sandbox *sandbox)
{
        assert(local_runqueue.add_fn_idx != NULL);
	/* wakeup worker if it is empty before we add a new request */

	if (!runtime_worker_busy_loop_enabled) {
		/*pthread_mutex_lock(&mutexs[index]);
		if (local_runqueue_is_empty_index(index)) {
			local_runqueue.add_fn_idx(index, sandbox);
                	//atomic_fetch_add(&local_runqueue_count[index], 1);
			pthread_mutex_unlock(&mutexs[index]);
			clock_gettime(CLOCK_MONOTONIC, &startT[index]);
                	pthread_cond_signal(&conds[index]);
		} else {
			pthread_mutex_unlock(&mutexs[index]);
			local_runqueue.add_fn_idx(index, sandbox);
		}*/

		if (local_runqueue_is_empty_index(index)) {
			local_runqueue.add_fn_idx(index, sandbox);
			//clock_gettime(CLOCK_MONOTONIC, &startT[index]);
			sem_post(&semlock[index]);	
		} else {
			local_runqueue.add_fn_idx(index, sandbox);
		}
	} else {
		local_runqueue.add_fn_idx(index, sandbox);
	}

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
	total_complete_requests++;
	local_runqueue.delete_fn(sandbox);
	//atomic_fetch_sub(&local_runqueue_count[global_worker_thread_idx], 1);
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
 * Checks if run queue is empty
 * @returns true if empty
 */
bool
local_runqueue_is_empty_index(int index)
{
        assert(local_runqueue.is_empty_fn_idx != NULL);
        return local_runqueue.is_empty_fn_idx(index);
}

/**
 * Get height if run queue is a binary search tree
 */

int local_runqueue_get_height() {
	assert(local_runqueue.get_height_fn != NULL);
	return local_runqueue.get_height_fn();
}

/**
 * Get total count of items in the queue 
 */
int local_runqueue_get_length() {
	assert(local_runqueue.get_length_fn != NULL);
	return local_runqueue.get_length_fn();
}
int local_runqueue_get_length_index(int index) {
	assert(local_runqueue.get_length_fn_idx != NULL);
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
        for (int i = 0; i < 1024; i++) {
		atomic_init(&worker_queuing_cost[i], 0);
		//atomic_init(&local_runqueue_count[i], 0);
	}
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

void 
wakeup_worker(int index) {
	pthread_mutex_lock(&mutexs[index]);
	pthread_cond_signal(&conds[index]);
	pthread_mutex_unlock(&mutexs[index]);
}
