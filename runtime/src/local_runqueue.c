
#ifdef LOG_LOCAL_RUNQUEUE
#include <stdint.h>
#endif
#include "memlogging.h"
#include "local_runqueue.h"
#include "admissions_control.h"

extern __thread int worker_thread_idx;
extern uint32_t runtime_processor_speed_MHz;
extern uint64_t system_start_timestamp;
static struct local_runqueue_config local_runqueue;

__thread uint32_t local_runqueue_count = 0;
#ifdef LOG_LOCAL_RUNQUEUE
#endif

__thread uint32_t local_workload_count = 0;
__thread uint32_t local_total_workload_count = 0;
/* The sum of requests count * requests' execution time */
__thread uint64_t local_realtime_workload_us = 0;  
__thread uint64_t local_total_realtime_workload_us = 0;  
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
	local_runqueue_count++;
#ifdef LOG_LOCAL_RUNQUEUE
#endif
	return local_runqueue.add_fn(sandbox);
}

/**
 * Delete a sandbox from the run queue
 * @param sandbox to delete
 */
void
local_runqueue_delete(struct sandbox *sandbox)
{
	assert(local_runqueue.delete_fn != NULL);
	local_runqueue_count--;
#ifdef LOG_LOCAL_RUNQUEUE
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
 * Get next sandbox from run queue, where next is defined by
 * @returns sandbox (or NULL?)
 */
struct sandbox *
local_runqueue_get_next()
{
	assert(local_runqueue.get_next_fn != NULL);
	return local_runqueue.get_next_fn();
};

/** 
 * The worker thread gets a new request, add the workload counter by 1
 */
void
local_workload_add(struct sandbox *sandbox) {
	assert(sandbox);
	uint64_t timestamp = __getcycles() - system_start_timestamp;
	local_total_workload_count++;
	local_workload_count++;
	uint64_t estimated_execution_time = admission_info_get_percentile(&sandbox->module->admissions_info);
	local_realtime_workload_us += estimated_execution_time / runtime_processor_speed_MHz;
	local_total_realtime_workload_us += estimated_execution_time / runtime_processor_speed_MHz;
	//mem_log("time %lu thread %d workload %u total workload %u real-time workload(us) %lu total real-time workload %lu\n", 
	//	timestamp, worker_thread_idx, local_workload_count, local_total_workload_count, local_realtime_workload_us,
	//	local_total_realtime_workload_us);
}

/**
 * One request is complete on the worker thread, and decrease the workload counter by 1
 */
void
local_workload_complete(struct sandbox *sandbox) {
	assert(sandbox);
	uint64_t timestamp = __getcycles() - system_start_timestamp;
	local_workload_count--;
	uint64_t estimated_execution_time = admission_info_get_percentile(&sandbox->module->admissions_info);
	if (local_realtime_workload_us < estimated_execution_time / runtime_processor_speed_MHz) {
		local_realtime_workload_us = 0;
	} else {
        	local_realtime_workload_us -= estimated_execution_time / runtime_processor_speed_MHz;
	}
        //mem_log("time %lu thread %d workload %u total workload %u real-time workload(us) %lu total real-time workload %lu\n", 
	//	timestamp, worker_thread_idx, local_workload_count, local_total_workload_count, local_realtime_workload_us,
	//	local_total_realtime_workload_us);

}
