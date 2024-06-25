#pragma once

#include <stdint.h>

#include "arch/context.h"
#include "http_session.h"
#include "module.h"
#include "ps_list.h"
#include "sandbox_state.h"
#include "sandbox_state_history.h"
#include "tenant.h"
#include "wasi.h"
#include "wasm_globals.h"
#include "wasm_memory.h"
#include "wasm_stack.h"
#include "wasm_types.h"

// #include "wasm_globals.h"
// #include "wasi.h"
// #include "listener_thread.h"
// #include "ck_ring.h"

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

struct sandbox_metadata;
struct sandbox {
	/* used by ps_list's default name-based MACROS for the scheduling runqueue */
	/* Keep as first member of sandbox struct to ensure ps_list maintains alignment */
	struct ps_list list;

	uint64_t                     id;
	sandbox_state_t              state;
	struct sandbox_state_history state_history;
	uint16_t                     response_code;
	
	size_t                       pq_idx_in_runqueue;
	size_t                       pq_idx_in_tenant_queue;
	int                          owned_worker_idx;
	int                          original_owner_worker_idx;
	int                          global_queue_type;
	uint8_t                      num_of_overshoots;
	struct sandbox_metadata     *sandbox_meta;


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

	// uint64_t sizes[1000000];
	// uint64_t sizesize;

	/* Scheduling and Temporal State */
	struct sandbox_timestamps timestamp_of;
	uint64_t                  duration_of_state[SANDBOX_STATE_COUNT];
	uint64_t                  last_state_duration;
	uint64_t                  last_running_state_duration;

	uint64_t remaining_exec;
	uint64_t absolute_deadline;
	bool     exceeded_estimation;
	bool     writeback_preemption_in_progress;
	bool     writeback_overshoot_in_progress;
	uint64_t admissions_estimate; /* estimated execution time (cycles) * runtime_admissions_granularity / relative
	                                 deadline (cycles) */
	uint64_t total_time;          /* Total time from Request to Response */
	int      payload_size;

	/* System Interface State */
	int32_t         return_value;
	wasi_context_t *wasi_context;
} PAGE_ALIGNED;

struct sandbox_metadata {
	struct sandbox        *sandbox_shadow;
	struct tenant         *tenant;
	struct route          *route;
	struct priority_queue *tenant_queue;
	uint64_t               id;
	uint64_t               allocation_timestamp;
	uint64_t               absolute_deadline;
	uint64_t               remaining_exec;
	uint64_t               total_be_exec_cycles;
	uint64_t               extra_slack; /* cycles */
	size_t                 pq_idx_in_tenant_queue;
	int                    owned_worker_idx;
	sandbox_state_t        state;
	bool                   exceeded_estimation;
	bool                   terminated;
	int                    global_queue_type;
	int                    worker_id_virt;
	uint16_t               error_code;

	struct job_node    *trs_job_node;
	struct demand_node *demand_node;
	struct demand_node *local_dbf_demand_node;
}; // PAGE_ALIGNED;
