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
#include "wasm_types.h"

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

struct sandbox_stack {
	void *   start; /* points to the bottom of the usable stack */
	uint32_t size;
};

struct sandbox_timestamps {
	uint64_t last_state_change; /* Used for bookkeeping of actual execution time */
	uint64_t request_arrival;   /* Timestamp when request is received */
	uint64_t allocation;        /* Timestamp when sandbox is allocated */
	uint64_t response;          /* Timestamp when response is sent */
	uint64_t completion;        /* Timestamp when sandbox runs to completion */
#ifdef LOG_SANDBOX_MEMORY_PROFILE
	uint32_t page_allocations[SANDBOX_PAGE_ALLOCATION_TIMESTAMP_COUNT];
	size_t   page_allocations_size;
#endif
};

struct sandbox {
	uint64_t        id;
	sandbox_state_t state;
	uint32_t sandbox_size; /* The struct plus enough buffer to hold the request or response (sized off largest) */

	/* WebAssembly Module State */
	struct module *module; /* the module this is an instance of */

	/* WebAssembly Instance State  */
	struct arch_context  ctxt;
	struct sandbox_stack stack;
	struct wasm_memory   memory;

	struct sandbox_timestamps timestamp_of;

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


	int32_t arguments_offset; /* actual placement of arguments in the sandbox. */
	void *  arguments;        /* arguments from request, must be of module->argument_count size. */
	int32_t return_value;

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
