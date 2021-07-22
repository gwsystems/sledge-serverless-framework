#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "arch/context.h"
#include "current_sandbox.h"
#include "ps_list.h"
#include "sandbox_request.h"
#include "sandbox_types.h"

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
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_ALLOCATED);
	assert(sandbox_request != NULL);
	assert(allocation_timestamp > 0);

	sandbox->id                  = sandbox_request->id;
	sandbox->admissions_estimate = sandbox_request->admissions_estimate;

	sandbox->request_arrival_timestamp = sandbox_request->request_arrival_timestamp;
	sandbox->enqueue_timestamp = sandbox_request->enqueue_timestamp;
	sandbox->allocation_timestamp      = allocation_timestamp;
	sandbox->state                     = SANDBOX_SET_AS_INITIALIZED;

	sandbox->request_from_outside = sandbox_request->request_from_outside;
	sandbox->previous_function_output = sandbox_request->previous_function_output;
	sandbox->output_length = sandbox_request->output_length;
	sandbox->pre_request_length = sandbox_request->pre_request_length;
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
