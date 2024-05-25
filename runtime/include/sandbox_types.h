#pragma once

#include <stdint.h>

#include "arch/context.h"
#include "http_session.h"
#include "module.h"
#include "ps_list.h"
#include "sandbox_state.h"
#include "sandbox_state_history.h"
#include "tenant.h"
#include "wasm_memory.h"
#include "wasm_types.h"
#include "wasm_stack.h"
#include "wasm_globals.h"
#include "wasi.h"

/*********************
 * Structs and Types *
 ********************/

struct sandbox_timestamps {
	uint64_t last_state_change; /* Used for bookkeeping of actual execution time */
	uint64_t allocation;        /* Timestamp when sandbox is allocated */
	uint64_t dispatched;        /* Timestamp when a sandbox is first added to a worker's runqueue */
	uint64_t completion;        /* Timestamp when sandbox runs to completion */
#ifdef LOG_SANDBOX_MEMORY_PROFILE
	uint32_t page_allocations[SANDBOX_PAGE_ALLOCATION_TIMESTAMP_COUNT];
	size_t   page_allocations_size;
#endif
};

struct sandbox {
	/* used by ps_list's default name-based MACROS for the scheduling runqueue */
	/* Keep as first member of sandbox struct to ensure ps_list maintains alignment */
	struct ps_list list;

	uint64_t                     id;
	sandbox_state_t              state;
	struct sandbox_state_history state_history;
	uint16_t                     response_code;


	/* Accounting Info */
	struct route  *route;
	struct tenant *tenant;

	/* HTTP State */
	struct http_session *http;

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
	uint64_t                  last_state_duration;

	uint64_t remaining_exec;
	uint64_t absolute_deadline;
	uint64_t admissions_estimate; /* estimated execution time (cycles) * runtime_admissions_granularity / relative
	                                 deadline (cycles) */
	uint64_t total_time;          /* Total time from Request to Response */
	int      payload_size;

	/* System Interface State */
	int32_t         return_value;
	wasi_context_t *wasi_context;

} PAGE_ALIGNED;
