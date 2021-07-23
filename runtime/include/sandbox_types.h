#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <ucontext.h>
#include <unistd.h>

#include "arch/context.h"
#include "http_parser.h"
#include "http_request.h"
#include "module.h"
#include "ps_list.h"
#include "sandbox_state.h"

#define SANDBOX_FILE_DESCRIPTOR_PREOPEN_MAGIC (707707707) /* upside down LOLLOLLOL 🤣😂🤣*/
#define SANDBOX_MAX_FD_COUNT                  32
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
	bool request_from_outside;
	int current_func_index; /* indicate the index of next function in the chain */
	char * previous_function_output; /* the output of the previous function */
	ssize_t output_length; /* the length of previous_function_output */
	ssize_t previosu_request_length; /* the length of previous request */
	sandbox_state_t state;

	uint32_t sandbox_size; /* The struct plus enough buffer to hold the request or response (sized off largest) */

	void *   linear_memory_start;    /* after sandbox struct */
	uint32_t linear_memory_size;     /* from after sandbox struct */
	uint64_t linear_memory_max_size; /* 4GB */

	void *   stack_start;
	uint32_t stack_size;

	struct arch_context ctxt; /* register context for context switch. */

	uint64_t request_arrival_timestamp;   /* Timestamp when request is received */
	uint64_t enqueue_timestamp;   /* Timestamp when sandbox request is enqueued to the global queue*/
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

	int             file_descriptors[SANDBOX_MAX_FD_COUNT];
	struct sockaddr client_address; /* client requesting connection! */
	int             client_socket_descriptor;

	bool                is_repeat_header;
	http_parser         http_parser;
	struct http_request http_request;

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
