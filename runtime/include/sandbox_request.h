#pragma once

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>

#include "debuglog.h"
#include "deque.h"
#include "http_total.h"
#include "module.h"
#include "runtime.h"
#include "sandbox_state.h"

struct sandbox_request {
	uint64_t        id;
	struct module * module;
	int             socket_descriptor;
	struct sockaddr socket_address;
	uint64_t        request_arrival_timestamp; /* cycles */
	uint64_t        absolute_deadline;         /* cycles */

	/*
	 * Unitless estimate of the instantaneous fraction of system capacity required to run the request
	 * Calculated by estimated execution time (cycles) * runtime_admissions_granularity / relative deadline (cycles)
	 */
	uint64_t admissions_estimate;
};

DEQUE_PROTOTYPE(sandbox, struct sandbox_request *)

/* Count of the total number of requests we've ever allocated. Never decrements as it is used to generate IDs */
extern _Atomic uint32_t sandbox_request_count;

static inline void
sandbox_request_count_initialize()
{
	atomic_init(&sandbox_request_count, 0);
}

static inline uint32_t
sandbox_request_count_postfix_increment()
{
	return atomic_fetch_add(&sandbox_request_count, 1);
}

static inline void
sandbox_request_log_allocation(struct sandbox_request *sandbox_request)
{
#ifdef LOG_REQUEST_ALLOCATION
	debuglog("Sandbox Request %lu: of %s:%d\n", sandbox_request->id, sandbox_request->module->name,
	         sandbox_request->module->port);
#endif
}

/**
 * Allocates a new Sandbox Request and places it on the Global Deque
 * @param module the module we want to request
 * @param socket_descriptor
 * @param socket_address
 * @param request_arrival_timestamp the timestamp of when we receives the request from the network (in cycles)
 * @return the new sandbox request
 */
static inline struct sandbox_request *
sandbox_request_allocate(struct module *module, int socket_descriptor, const struct sockaddr *socket_address,
                         uint64_t request_arrival_timestamp, uint64_t admissions_estimate)
{
	struct sandbox_request *sandbox_request = (struct sandbox_request *)malloc(sizeof(struct sandbox_request));
	assert(sandbox_request);

	/* Sets the ID to the value before the increment */
	sandbox_request->id = sandbox_request_count_postfix_increment();

	sandbox_request->module            = module;
	sandbox_request->socket_descriptor = socket_descriptor;
	memcpy(&sandbox_request->socket_address, socket_address, sizeof(struct sockaddr));
	sandbox_request->request_arrival_timestamp = request_arrival_timestamp;
	sandbox_request->absolute_deadline         = request_arrival_timestamp + module->relative_deadline;

	/*
	 * Admissions Control State
	 * Assumption: an estimate of 0 should have been interpreted as a rejection
	 */
	assert(admissions_estimate != 0);
	sandbox_request->admissions_estimate = admissions_estimate;

	sandbox_request_log_allocation(sandbox_request);

	return sandbox_request;
}
