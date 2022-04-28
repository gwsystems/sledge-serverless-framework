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
#include "sandbox_state_history.h"
#include "vec.h"
#include "wasm_memory.h"
#include "wasm_types.h"
#include "wasm_stack.h"
#include "wasm_globals.h"
#include "wasi.h"

#define u8 uint8_t
VEC(u8)

/*********************
 * Structs and Types *
 ********************/

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
	uint64_t                     id;
	sandbox_state_t              state;
	struct sandbox_state_history state_history;

	struct ps_list list; /* used by ps_list's default name-based MACROS for the scheduling runqueue */

	/* HTTP State */
	struct sockaddr     client_address; /* client requesting connection! */
	int                 client_socket_descriptor;
	http_parser         http_parser;
	struct http_request http_request;
	struct vec_u8       request;
	struct vec_u8       response;

	/* WebAssembly Module State */
	struct module *module; /* the module this is an instance of */

	/* WebAssembly Instance State  */
	struct arch_context      ctxt;
	struct wasm_stack       *stack;
	struct wasm_memory      *memory;
	struct vec_wasm_global_t globals;

	/* Scheduling and Temporal State */
	struct sandbox_timestamps timestamp_of;
	uint64_t                  duration_of_state[SANDBOX_STATE_COUNT];
	uint64_t                  last_duration_of_exec;

	uint64_t absolute_deadline;
	uint64_t admissions_estimate; /* estimated execution time (cycles) * runtime_admissions_granularity / relative
	                                 deadline (cycles) */
	uint64_t total_time;          /* Total time from Request to Response */

	/* System Interface State */
	int32_t         return_value;
	wasi_context_t *wasi_context;

} PAGE_ALIGNED;
