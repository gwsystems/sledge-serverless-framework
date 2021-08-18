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

#ifdef LOG_SANDBOX_MEMORY_PROFILE
#define SANDBOX_PAGE_ALLOCATION_TIMESTAMP_COUNT 1024
#endif

/*********************
 * Structs and Types *
 ********************/

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

/*
 * In-memory buffer used to read requests, buffer write to STDOUT, and write HTTP responses
 * The HTTP request is read in, updating buffer.length and http_request_length
 * --------------------------------------------------
 * | Request        | Empty                         |
 * --------------------------------------------------
 * Writes to STDOUT are written starting at http_request_length, updating buffer.length
 * --------------------------------------------------
 * | Request        | STDOUT  |  Empty              |
 * --------------------------------------------------
 * The HTTP Response is written over the Request (assumes the response is smaller)
 * --------------------------------------------------
 * | Response | Gap | STDOUT  |  Empty              |
 * --------------------------------------------------
 * And the STDOUT buffer is compacted to immediately follow the response
 * --------------------------------------------------
 * | Response | STDOUT  |  Empty                    |
 * --------------------------------------------------
 */
struct sandbox_buffer {
	ssize_t length; /* Should be <= module->max_request_or_response_size */
	char    start[1];
};

struct sandbox {
	uint64_t        id;
	sandbox_state_t state;
	uint32_t sandbox_size; /* The struct plus enough buffer to hold the request or response (sized off largest) */
	struct ps_list list;   /* used by ps_list's default name-based MACROS for the scheduling runqueue */

	/* HTTP State */
	struct sockaddr     client_address; /* client requesting connection! */
	int                 client_socket_descriptor;
	http_parser         http_parser;
	struct http_request http_request;
	ssize_t             http_request_length;

	/* WebAssembly Module State */
	struct module *module; /* the module this is an instance of */

	/* WebAssembly Instance State  */
	struct arch_context  ctxt;
	struct sandbox_stack stack;
	struct wasm_memory   memory;

	/* Scheduling and Temporal State */
	struct sandbox_timestamps      timestamp_of;
	struct sandbox_state_durations duration_of_state;

	uint64_t absolute_deadline;
	uint64_t admissions_estimate; /* estimated execution time (cycles) * runtime_admissions_granularity / relative
	                                 deadline (cycles) */
	uint64_t total_time;          /* Total time from Request to Response */

	/* System Interface State */
	int32_t arguments_offset; /* actual placement of arguments in the sandbox. */
	int32_t return_value;

	/* This contains a Variable Length Array and thus MUST be the final member of this struct */
	struct sandbox_buffer buffer;
} PAGE_ALIGNED;
