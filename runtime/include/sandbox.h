#pragma once

#include <ucontext.h>
#include <stdbool.h>

#include "arch/context.h"
#include "client_socket.h"
#include "deque.h"
#include "http_request.h"
#include "http_response.h"
#include "module.h"
#include "ps_list.h"
#include "sandbox_request.h"
#include "sandbox_state.h"
#include "software_interrupt.h"

#define SANDBOX_FILE_DESCRIPTOR_PREOPEN_MAGIC (707707707) /* upside down LOLLOLLOL 🤣😂🤣*/
#define SANDBOX_MAX_IO_HANDLE_COUNT           32
#define SANDBOX_MAX_MEMORY                    (1L << 32) /* 4GB */

#ifdef LOG_SANDBOX_MEMORY_PROFILE
#define SANDBOX_PAGE_ALLOCATION_TIMESTAMP_COUNT 1024
#endif

/*********************
 * Structs and Types *
 ********************/

struct sandbox_io_handle {
	int file_descriptor;
};

struct sandbox {
	uint64_t        id;
	sandbox_state_t state;

	uint32_t sandbox_size; /* The struct plus enough buffer to hold the request or response (sized off largest) */

	void *   linear_memory_start;    /* after sandbox struct */
	uint32_t linear_memory_size;     /* from after sandbox struct */
	uint64_t linear_memory_max_size; /* 4GB */

	void *   stack_start;
	uint32_t stack_size;

	struct arch_context ctxt; /* register context for context switch. */

	uint64_t request_arrival_timestamp;   /* Timestamp when request is received */
	uint64_t allocation_timestamp;        /* Timestamp when sandbox is allocated */
	uint64_t response_timestamp;          /* Timestamp when response is sent */
	uint64_t completion_timestamp;        /* Timestamp when sandbox runs to completion */
	uint64_t last_state_change_timestamp; /* Used for bookkeeping of actual execution time */
#ifdef LOG_SANDBOX_MEMORY_PROFILE
	uint32_t page_allocation_timestamps[SANDBOX_PAGE_ALLOCATION_TIMESTAMP_COUNT];
	size_t   page_allocation_timestamps_size;
#endif
	/* Duration of time (in cycles) that the sandbox is in each state */
	uint64_t initializing_duration;
	uint64_t runnable_duration;
	uint64_t preempted_duration;
	uint64_t running_duration;
	uint64_t blocked_duration;
	uint64_t returned_duration;

	uint64_t absolute_deadline;
	uint64_t total_time; /* From Request to Response */

	/*
	 * Unitless estimate of the instantaneous fraction of system capacity required to run the request
	 * Calculated by estimated execution time (cycles) * runtime_admissions_granularity / relative deadline (cycles)
	 */
	uint64_t admissions_estimate;

	struct module *module; /* the module this is an instance of */

	int32_t arguments_offset; /* actual placement of arguments in the sandbox. */
	void *  arguments;        /* arguments from request, must be of module->argument_count size. */
	int32_t return_value;

	struct sandbox_io_handle io_handles[SANDBOX_MAX_IO_HANDLE_COUNT];
	struct sockaddr          client_address; /* client requesting connection! */
	int                      client_socket_descriptor;

	bool                 is_repeat_header;
	http_parser          http_parser;
	struct http_request  http_request;
	struct http_response http_response;

	char *  read_buffer;
	ssize_t read_length, read_size;

	/* Used for the scheduling runqueue as an in-place linked list data structure. */
	/* The variable name "list" is used for ps_list's default name-based MACROS. */
	struct ps_list list;

	/*
	 * The length of the HTTP Request.
	 * This acts as an offset to the STDOUT of the Sandbox
	 */
	ssize_t request_length;

	ssize_t request_response_data_length; /* Should be <= module->max_request_or_response_size */
	char    request_response_data[1];     /* of request_response_data_length, following sandbox mem.. */
} PAGE_ALIGNED;

/***************************
 * Public API              *
 **************************/

struct sandbox *sandbox_allocate(struct sandbox_request *sandbox_request);
void            sandbox_free(struct sandbox *sandbox);
void            sandbox_free_linear_memory(struct sandbox *sandbox);
void            sandbox_main(struct sandbox *sandbox);


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
 * Getter for the arguments of the sandbox
 * @param sandbox
 * @return the arguments of the sandbox
 */
static inline char *
sandbox_get_arguments(struct sandbox *sandbox)
{
	if (!sandbox) return NULL;
	return (char *)sandbox->arguments;
}

/**
 * Initializes and returns an IO handle on the current sandbox ready for use
 * @param sandbox
 * @return index of handle we preopened or -1 on error (sandbox is null or all io_handles are exhausted)
 */
static inline int
sandbox_initialize_io_handle(struct sandbox *sandbox)
{
	if (!sandbox) return -1;
	int io_handle_index;
	for (io_handle_index = 0; io_handle_index < SANDBOX_MAX_IO_HANDLE_COUNT; io_handle_index++) {
		if (sandbox->io_handles[io_handle_index].file_descriptor < 0) break;
	}
	if (io_handle_index == SANDBOX_MAX_IO_HANDLE_COUNT) return -1;
	sandbox->io_handles[io_handle_index].file_descriptor = SANDBOX_FILE_DESCRIPTOR_PREOPEN_MAGIC;
	return io_handle_index;
}

/**
 * Sets the file descriptor of the sandbox's ith io_handle
 * Returns error condition if the file_descriptor to set does not contain sandbox preopen magic
 * @param sandbox
 * @param io_handle_index index of the sandbox io_handles we want to set
 * @param file_descriptor the file descripter we want to set it to
 * @returns the index that was set or -1 in case of error
 */
static inline int
sandbox_set_file_descriptor(struct sandbox *sandbox, int io_handle_index, int file_descriptor)
{
	if (!sandbox) return -1;
	if (io_handle_index >= SANDBOX_MAX_IO_HANDLE_COUNT || io_handle_index < 0) return -1;
	if (file_descriptor < 0
	    || sandbox->io_handles[io_handle_index].file_descriptor != SANDBOX_FILE_DESCRIPTOR_PREOPEN_MAGIC)
		return -1;
	sandbox->io_handles[io_handle_index].file_descriptor = file_descriptor;
	return io_handle_index;
}

/**
 * Get the file descriptor of the sandbox's ith io_handle
 * @param sandbox
 * @param io_handle_index index into the sandbox's io_handles table
 * @returns file descriptor or -1 in case of error
 */
static inline int
sandbox_get_file_descriptor(struct sandbox *sandbox, int io_handle_index)
{
	if (!sandbox) return -1;
	if (io_handle_index >= SANDBOX_MAX_IO_HANDLE_COUNT || io_handle_index < 0) return -1;
	return sandbox->io_handles[io_handle_index].file_descriptor;
}

/**
 * Close the sandbox's ith io_handle
 * @param sandbox
 * @param io_handle_index index of the handle to close
 */
static inline void
sandbox_close_file_descriptor(struct sandbox *sandbox, int io_handle_index)
{
	if (io_handle_index >= SANDBOX_MAX_IO_HANDLE_COUNT || io_handle_index < 0) return;
	/* TODO: Do we actually need to call some sort of close function here? Issue #90 */
	sandbox->io_handles[io_handle_index].file_descriptor = -1;
}

void sandbox_close_http(struct sandbox *sandbox);
void sandbox_print_perf(struct sandbox *sandbox);
void sandbox_summarize_page_allocations(struct sandbox *sandbox);

INLINE void sandbox_set_as_initialized(struct sandbox *sandbox, struct sandbox_request *sandbox_request,
                                       uint64_t allocation_timestamp);
INLINE void sandbox_set_as_runnable(struct sandbox *sandbox, sandbox_state_t last_state);
INLINE void sandbox_set_as_running(struct sandbox *sandbox, sandbox_state_t last_state);
INLINE void sandbox_set_as_blocked(struct sandbox *sandbox, sandbox_state_t last_state);
INLINE void sandbox_set_as_preempted(struct sandbox *sandbox, sandbox_state_t last_state);
INLINE void sandbox_set_as_returned(struct sandbox *sandbox, sandbox_state_t last_state);
INLINE void sandbox_set_as_complete(struct sandbox *sandbox, sandbox_state_t last_state);
INLINE void sandbox_set_as_error(struct sandbox *sandbox, sandbox_state_t last_state);
