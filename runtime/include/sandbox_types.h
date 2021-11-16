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

#ifdef LOG_STATE_CHANGES
#define SANDBOX_STATE_HISTORY_CAPACITY 100
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
	uint64_t last_preemption;   /* Timestamp when sandbox is last preempted or blocked */
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
 * Static In-memory buffers are used for HTTP requests read in via STDIN and HTTP
 * responses written back out via STDOUT. These are allocated in pages immediately
 * adjacent to the sandbox struct in the following layout. The capacity of these
 * buffers are configured in the module spec and stored in sandbox->module.max_request_size
 * and sandbox->module.max_response_size.
 *
 * Because the sandbox struct, the request header, and the response header are sized
 * in pages, we must store the base pointer to the buffer. The length is increased
 * and should not exceed the respective module max size.
 *
 * ---------------------------------------------------
 * | Sandbox | Request         | Response            |
 * ---------------------------------------------------
 *
 * After the sandbox writes its response, a header is written at a negative offset
 * overwriting the tail end of the request buffer. This assumes that the request
 * data is no longer needed because the sandbox has run to completion
 *
 * ---------------------------------------------------
 * | Sandbox | Garbage   | HDR | Response            |
 * ---------------------------------------------------
 */
struct sandbox_buffer {
	char * base;
	size_t length;
};

struct sandbox {
	uint64_t        id;
	sandbox_state_t state;

#ifdef LOG_STATE_CHANGES
	sandbox_state_t state_history[SANDBOX_STATE_HISTORY_CAPACITY];
	uint16_t        state_history_count;
#endif

	struct ps_list list; /* used by ps_list's default name-based MACROS for the scheduling runqueue */

	/* HTTP State */
	struct sockaddr       client_address; /* client requesting connection! */
	int                   client_socket_descriptor;
	http_parser           http_parser;
	struct http_request   http_request;
	ssize_t               http_request_length; /* TODO: Get rid of me */
	struct sandbox_buffer request;
	struct sandbox_buffer response;

	/* WebAssembly Module State */
	struct module *module; /* the module this is an instance of */

	/* WebAssembly Instance State  */
	struct arch_context  ctxt;
	struct sandbox_stack stack;
	struct wasm_memory   memory;

	/* Scheduling and Temporal State */
	struct sandbox_timestamps timestamp_of;
	uint64_t                  duration_of_state[SANDBOX_STATE_COUNT];

	uint64_t absolute_deadline;
	uint64_t admissions_estimate; /* estimated execution time (cycles) * runtime_admissions_granularity / relative
	                                 deadline (cycles) */
	uint64_t total_time;          /* Total time from Request to Response */

	/* System Interface State */
	int32_t arguments_offset; /* actual placement of arguments in the sandbox. */
	int32_t return_value;

} PAGE_ALIGNED;
