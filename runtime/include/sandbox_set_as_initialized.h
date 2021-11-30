#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "arch/context.h"
#include "current_sandbox.h"
#include "ps_list.h"
#include "sandbox_request.h"
#include "sandbox_state_history.h"
#include "sandbox_types.h"

/**
 * Transitions a sandbox to the SANDBOX_INITIALIZED state.
 * The sandbox was already zeroed out during allocation
 * @param sandbox an uninitialized sandbox
 * @param sandbox_request the request we are initializing the sandbox from
 * @param allocation_timestamp timestamp of allocation
 */
static inline void
sandbox_set_as_initialized(struct sandbox *self, struct sandbox_request *sandbox_request, uint64_t allocation_timestamp)
{
	assert(self);
	assert(self->state == SANDBOX_UNINITIALIZED);
	assert(self != NULL);
	assert(allocation_timestamp > 0);
	uint64_t now = __getcycles();

	self->id    = sandbox_request->id;
	self->state = SANDBOX_INITIALIZED;

#ifdef LOG_STATE_CHANGES
	sandbox_state_history_append(self, SANDBOX_UNINITIALIZED);
	memset(&sandbox->state_history, 0, SANDBOX_STATE_HISTORY_CAPACITY * sizeof(sandbox_state_t));
	sandbox->state_history_count                           = 0;
	sandbox->state_history[sandbox->state_history_count++] = SANDBOX_UNINITIALIZED;
#endif

#ifdef LOG_SANDBOX_MEMORY_PROFILE
	self->timestamp_of.page_allocations_size = 0;
#endif

	ps_list_init_d(self);

	self->absolute_deadline            = sandbox_request->absolute_deadline;
	self->admissions_estimate          = sandbox_request->admissions_estimate;
	self->client_socket_descriptor     = sandbox_request->socket_descriptor;
	self->timestamp_of.request_arrival = sandbox_request->request_arrival_timestamp;
	/* Copy the socket descriptor and address of the client invocation */
	memcpy(&self->client_address, &sandbox_request->socket_address, sizeof(struct sockaddr));

	/* Allocations require the module to be set */
	self->module = sandbox_request->module;
	module_acquire(self->module);

	memset(&self->duration_of_state, 0, SANDBOX_STATE_COUNT * sizeof(uint64_t));

	/* State Change Bookkeeping */
	self->duration_of_state[SANDBOX_UNINITIALIZED] = now - allocation_timestamp;
	self->timestamp_of.allocation                  = allocation_timestamp;
	self->timestamp_of.last_state_change           = allocation_timestamp;
	sandbox_state_history_append(self, SANDBOX_INITIALIZED);
	runtime_sandbox_total_increment(SANDBOX_INITIALIZED);
}
