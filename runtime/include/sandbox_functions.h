#pragma once

#include <ucontext.h>
#include <stdbool.h>

#include "arch/context.h"
#include "client_socket.h"
#include "current_sandbox.h"
#include "deque.h"
#include "http_parser.h"
#include "http_request.h"
#include "local_completion_queue.h"
#include "local_runqueue.h"
#include "module.h"
#include "ps_list.h"
#include "sandbox_request.h"
#include "sandbox_state.h"
#include "sandbox_types.h"
#include "software_interrupt.h"

extern void current_sandbox_start(void);

/***************************
 * Public API              *
 **************************/

struct sandbox *sandbox_allocate(struct sandbox_request *sandbox_request);
void            sandbox_close_http(struct sandbox *sandbox);
void            sandbox_free(struct sandbox *sandbox);
void            sandbox_free_linear_memory(struct sandbox *sandbox);
int             sandbox_initialize_file_descriptor(struct sandbox *sandbox);
void            sandbox_main(struct sandbox *sandbox);
void            sandbox_switch_to(struct sandbox *next_sandbox);

/**
 * Given a sandbox, returns the module that sandbox is executing
 * @param sandbox the sandbox whose module we want
 * @return the module of the provided sandbox
 */
static inline struct module *
sandbox_get_module(struct sandbox *sandbox)
{
	if (!sandbox) return NULL;
	return sandbox->module;
}

/**
 * Resolve a sandbox's fd to the host fd it maps to
 * @param sandbox
 * @param fd_index index into the sandbox's fd table
 * @returns file descriptor or -1 in case of error
 */
static inline int
sandbox_get_file_descriptor(struct sandbox *sandbox, int fd_index)
{
	if (!sandbox) return -1;
	if (fd_index >= SANDBOX_MAX_FD_COUNT || fd_index < 0) return -1;
	return sandbox->file_descriptors[fd_index];
}

static inline uint64_t
sandbox_get_priority(void *element)
{
	struct sandbox *sandbox = (struct sandbox *)element;
	return sandbox->absolute_deadline;
};

/**
 * Maps a sandbox fd to an underlying host fd
 * Returns error condition if the file_descriptor to set does not contain sandbox preopen magic
 * @param sandbox
 * @param sandbox_fd index of the sandbox fd we want to set
 * @param file_descriptor the file descripter we want to set it to
 * @returns the index that was set or -1 in case of error
 */
static inline int
sandbox_set_file_descriptor(struct sandbox *sandbox, int sandbox_fd, int host_fd)
{
	if (!sandbox) return -1;
	if (sandbox_fd >= SANDBOX_MAX_FD_COUNT || sandbox_fd < 0) return -1;
	if (host_fd < 0 || sandbox->file_descriptors[sandbox_fd] != SANDBOX_FILE_DESCRIPTOR_PREOPEN_MAGIC) return -1;
	sandbox->file_descriptors[sandbox_fd] = host_fd;
	return sandbox_fd;
}

/**
 * Map the host stdin, stdout, stderr to the sandbox
 * @param sandbox - the sandbox on which we are initializing stdio
 */
static inline void
sandbox_initialize_stdio(struct sandbox *sandbox)
{
	int sandbox_fd, rc;
	for (int host_fd = 0; host_fd <= 2; host_fd++) {
		sandbox_fd = sandbox_initialize_file_descriptor(sandbox);
		assert(sandbox_fd == host_fd);
		rc = sandbox_set_file_descriptor(sandbox, sandbox_fd, host_fd);
		assert(rc != -1);
	}
}

static inline void
sandbox_open_http(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	http_parser_init(&sandbox->http_parser, HTTP_REQUEST);

	/* Set the sandbox as the data the http-parser has access to */
	sandbox->http_parser.data = sandbox;

	/* Freshly allocated sandbox going runnable for first time, so register client socket with epoll */
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)sandbox;
	accept_evt.events   = EPOLLIN | EPOLLOUT | EPOLLET;
	int rc = epoll_ctl(worker_thread_epoll_file_descriptor, EPOLL_CTL_ADD, sandbox->client_socket_descriptor,
	                   &accept_evt);
	if (unlikely(rc < 0)) panic_err();
}

/**
 * Prints key performance metrics for a sandbox to runtime_sandbox_perf_log
 * This is defined by an environment variable
 * @param sandbox
 */
static inline void
sandbox_print_perf(struct sandbox *sandbox)
{
	/* If the log was not defined by an environment variable, early out */
	if (runtime_sandbox_perf_log == NULL) return;

	uint32_t total_time_us = sandbox->total_time / runtime_processor_speed_MHz;
	uint32_t queued_us     = (sandbox->allocation_timestamp - sandbox->request_arrival_timestamp)
	                     / runtime_processor_speed_MHz;
	uint32_t initializing_us = sandbox->initializing_duration / runtime_processor_speed_MHz;
	uint32_t runnable_us     = sandbox->runnable_duration / runtime_processor_speed_MHz;
	uint32_t running_us      = sandbox->running_duration / runtime_processor_speed_MHz;
	uint32_t blocked_us      = sandbox->blocked_duration / runtime_processor_speed_MHz;
	uint32_t returned_us     = sandbox->returned_duration / runtime_processor_speed_MHz;

	/*
	 * Assumption: A sandbox is never able to free pages. If linear memory management
	 * becomes more intelligent, then peak linear memory size needs to be tracked
	 * seperately from current linear memory size.
	 */
	fprintf(runtime_sandbox_perf_log, "%lu,%s():%d,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", sandbox->id,
	        sandbox->module->name, sandbox->module->port, sandbox_state_stringify(sandbox->state),
	        sandbox->module->relative_deadline_us, total_time_us, queued_us, initializing_us, runnable_us,
	        running_us, blocked_us, returned_us, sandbox->linear_memory_size);
}

static inline void
sandbox_summarize_page_allocations(struct sandbox *sandbox)
{
#ifdef LOG_SANDBOX_MEMORY_PROFILE
	// TODO: Handle interleavings
	char sandbox_page_allocations_log_path[100] = {};
	sandbox_page_allocations_log_path[99]       = '\0';
	snprintf(sandbox_page_allocations_log_path, 99, "%s_%d_page_allocations.csv", sandbox->module->name,
	         sandbox->module->port);

	debuglog("Logging to %s", sandbox_page_allocations_log_path);

	FILE *sandbox_page_allocations_log = fopen(sandbox_page_allocations_log_path, "a");

	fprintf(sandbox_page_allocations_log, "%lu,%lu,%s,", sandbox->id, sandbox->running_duration,
	        sandbox_state_stringify(sandbox->state));
	for (size_t i = 0; i < sandbox->page_allocation_timestamps_size; i++)
		fprintf(sandbox_page_allocations_log, "%u,", sandbox->page_allocation_timestamps[i]);

	fprintf(sandbox_page_allocations_log, "\n");
#else
	return;
#endif
}

/**
 * Transitions a sandbox to the SANDBOX_INITIALIZED state.
 * The sandbox was already zeroed out during allocation
 * @param sandbox an uninitialized sandbox
 * @param sandbox_request the request we are initializing the sandbox from
 * @param allocation_timestamp timestamp of allocation
 */
static inline void
sandbox_set_as_initialized(struct sandbox *sandbox, struct sandbox_request *sandbox_request,
                           uint64_t allocation_timestamp)
{
	assert(!software_interrupt_is_enabled());
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_ALLOCATED);
	assert(sandbox_request != NULL);
	assert(allocation_timestamp > 0);

	sandbox->id                  = sandbox_request->id;
	sandbox->admissions_estimate = sandbox_request->admissions_estimate;

	sandbox->request_arrival_timestamp = sandbox_request->request_arrival_timestamp;
	sandbox->allocation_timestamp      = allocation_timestamp;
	sandbox->state                     = SANDBOX_SET_AS_INITIALIZED;

	/* Initialize the sandbox's context, stack, and instruction pointer */
	/* stack_start points to the bottom of the usable stack, so add stack_size to get to top */
	arch_context_init(&sandbox->ctxt, (reg_t)current_sandbox_start,
	                  (reg_t)sandbox->stack_start + sandbox->stack_size);

	/* Mark sandbox fds as invalid by setting to -1 */
	for (int i = 0; i < SANDBOX_MAX_FD_COUNT; i++) sandbox->file_descriptors[i] = -1;

	/* Initialize Parsec control structures */
	ps_list_init_d(sandbox);

	/* Copy the socket descriptor, address, and arguments of the client invocation */
	sandbox->absolute_deadline        = sandbox_request->absolute_deadline;
	sandbox->arguments                = (void *)sandbox_request->arguments;
	sandbox->client_socket_descriptor = sandbox_request->socket_descriptor;
	memcpy(&sandbox->client_address, &sandbox_request->socket_address, sizeof(struct sockaddr));

	sandbox->last_state_change_timestamp = allocation_timestamp; /* We use arg to include alloc */
	sandbox->state                       = SANDBOX_INITIALIZED;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, SANDBOX_UNINITIALIZED, SANDBOX_INITIALIZED);
	runtime_sandbox_total_increment(SANDBOX_INITIALIZED);
}

/**
 * Transitions a sandbox to the SANDBOX_RUNNABLE state.
 *
 * This occurs in the following scenarios:
 * - A sandbox in the SANDBOX_INITIALIZED state completes initialization and is ready to be run
 * - A sandbox in the SANDBOX_BLOCKED state completes what was blocking it and is ready to be run
 *
 * @param sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_runnable(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_RUNNABLE;

	switch (last_state) {
	case SANDBOX_INITIALIZED: {
		sandbox->initializing_duration += duration_of_last_state;
		break;
	}
	case SANDBOX_BLOCKED: {
		sandbox->blocked_duration += duration_of_last_state;
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Runnable\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	local_runqueue_add(sandbox);
	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RUNNABLE;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_RUNNABLE);
	runtime_sandbox_total_increment(SANDBOX_RUNNABLE);
	runtime_sandbox_total_decrement(last_state);
}

static inline void
sandbox_set_as_running(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_RUNNING;

	switch (last_state) {
	case SANDBOX_RUNNABLE: {
		sandbox->runnable_duration += duration_of_last_state;
		break;
	}
	case SANDBOX_PREEMPTED: {
		sandbox->preempted_duration += duration_of_last_state;
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Running\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	current_sandbox_set(sandbox);
	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RUNNING;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_RUNNING);
	runtime_sandbox_total_increment(SANDBOX_RUNNING);
	runtime_sandbox_total_decrement(last_state);
}

/**
 * Transitions a sandbox to the SANDBOX_BLOCKED state.
 * This occurs when a sandbox is executing and it makes a blocking API call of some kind.
 * Automatically removes the sandbox from the runqueue
 * @param sandbox the blocking sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_blocked(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_BLOCKED;

	switch (last_state) {
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		local_runqueue_delete(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Blocked\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_BLOCKED;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_BLOCKED);
	runtime_sandbox_total_increment(SANDBOX_BLOCKED);
	runtime_sandbox_total_decrement(last_state);
}

/**
 * Transitions a sandbox to the SANDBOX_PREEMPTED state.
 *
 * This occurs when a sandbox is executing and in a RUNNING state and a SIGALRM software interrupt fires
 * and pulls a sandbox with an earlier absolute deadline from the global request scheduler.
 *
 * @param sandbox the sandbox being preempted
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_preempted(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_PREEMPTED;

	switch (last_state) {
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Preempted\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_PREEMPTED;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_PREEMPTED);
	runtime_sandbox_total_increment(SANDBOX_PREEMPTED);
	runtime_sandbox_total_decrement(SANDBOX_RUNNING);
}

/**
 * Transitions a sandbox to the SANDBOX_RETURNED state.
 * This occurs when a sandbox is executing and runs to completion.
 * Automatically removes the sandbox from the runqueue and unmaps linear memory.
 * Because the stack is still in use, freeing the stack is deferred until later
 * @param sandbox the blocking sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_returned(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_RETURNED;

	switch (last_state) {
	case SANDBOX_RUNNING: {
		sandbox->response_timestamp = now;
		sandbox->total_time         = now - sandbox->request_arrival_timestamp;
		sandbox->running_duration += duration_of_last_state;
		local_runqueue_delete(sandbox);
		sandbox_free_linear_memory(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Returned\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RETURNED;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_RETURNED);
	runtime_sandbox_total_increment(SANDBOX_RETURNED);
	runtime_sandbox_total_decrement(last_state);
}

/**
 * Transitions a sandbox from the SANDBOX_RETURNED state to the SANDBOX_COMPLETE state.
 * Adds the sandbox to the completion queue
 * @param sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_complete(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_COMPLETE;

	switch (last_state) {
	case SANDBOX_RETURNED: {
		sandbox->completion_timestamp = now;
		sandbox->returned_duration += duration_of_last_state;
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Error\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	uint64_t sandbox_id = sandbox->id;
	sandbox->state      = SANDBOX_COMPLETE;
	sandbox_print_perf(sandbox);
	sandbox_summarize_page_allocations(sandbox);
	/* Admissions Control Post Processing */
	admissions_info_update(&sandbox->module->admissions_info, sandbox->running_duration);
	admissions_control_subtract(sandbox->admissions_estimate);
	/* Do not touch sandbox state after adding to completion queue to avoid use-after-free bugs */
	local_completion_queue_add(sandbox);

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox_id, last_state, SANDBOX_COMPLETE);
	runtime_sandbox_total_increment(SANDBOX_COMPLETE);
	runtime_sandbox_total_decrement(last_state);
}

/**
 * Transitions a sandbox to the SANDBOX_ERROR state.
 * This can occur during initialization or execution
 * Unmaps linear memory, removes from the runqueue (if on it), and adds to the completion queue
 * Because the stack is still in use, freeing the stack is deferred until later
 *
 * TODO: Is the sandbox adding itself to the completion queue here? Is this a problem? Issue #94
 *
 * @param sandbox the sandbox erroring out
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
static inline void
sandbox_set_as_error(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_ERROR;

	switch (last_state) {
	case SANDBOX_SET_AS_INITIALIZED:
		/* Technically, this is a degenerate sandbox that we generate by hand */
		sandbox->initializing_duration += duration_of_last_state;
		break;
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		local_runqueue_delete(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Error\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	uint64_t sandbox_id = sandbox->id;
	sandbox->state      = SANDBOX_ERROR;
	sandbox_print_perf(sandbox);
	sandbox_summarize_page_allocations(sandbox);
	sandbox_free_linear_memory(sandbox);
	admissions_control_subtract(sandbox->admissions_estimate);
	/* Do not touch sandbox after adding to completion queue to avoid use-after-free bugs */
	local_completion_queue_add(sandbox);

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox_id, last_state, SANDBOX_ERROR);
	runtime_sandbox_total_increment(SANDBOX_ERROR);
	runtime_sandbox_total_decrement(last_state);
}

/**
 * Conditionally triggers appropriate state changes for exiting sandboxes
 * @param exiting_sandbox - The sandbox that ran to completion
 */
static inline void
sandbox_exit(struct sandbox *exiting_sandbox)
{
	assert(exiting_sandbox != NULL);

	switch (exiting_sandbox->state) {
	case SANDBOX_RETURNED:
		/*
		 * We draw a distinction between RETURNED and COMPLETED because a sandbox cannot add itself to the
		 * completion queue
		 */
		sandbox_set_as_complete(exiting_sandbox, SANDBOX_RETURNED);
		break;
	case SANDBOX_BLOCKED:
		/* Cooperative yield, so just break */
		break;
	case SANDBOX_ERROR:
		/* Terminal State, so just break */
		break;
	default:
		panic("Cooperatively switching from a sandbox in a non-terminal %s state\n",
		      sandbox_state_stringify(exiting_sandbox->state));
	}
}

/**
 * Mark a blocked sandbox as runnable and add it to the runqueue
 * @param sandbox the sandbox to check and update if blocked
 */
static inline void
sandbox_wakeup(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_BLOCKED);

	software_interrupt_disable();
	sandbox_set_as_runnable(sandbox, SANDBOX_BLOCKED);
	software_interrupt_enable();
}
