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
	uint64_t cleanup;	    /* Time duration of cleaning up the previous sandboxes */
	uint64_t other;	    	    /* Time duration of only sandbox_free */
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

	uint64_t absolute_deadline;
	uint64_t admissions_estimate; /* estimated execution time (cycles) * runtime_admissions_granularity / relative
	                                 deadline (cycles) */
	uint64_t total_time;          /* Total time from Request to Response */

	void *rpc_handler;
	uint8_t rpc_id;
	uint8_t *rpc_request_body;
	size_t rpc_request_body_size;
	/* Runtime state used by WASI */
        int cursor; /* Sandbox cursor (offset from body pointer) */
	struct auto_buf         response_body;
	//size_t                  response_body_written;

	/* System Interface State */
	int32_t         return_value;
	wasi_context_t *wasi_context;
	int context_switch_to; /* 1 means context switch to base, 2 means context swtich to next sandbox */
	uint64_t ret[5];
} PAGE_ALIGNED;
