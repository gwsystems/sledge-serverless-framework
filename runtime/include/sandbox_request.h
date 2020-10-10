#pragma once

#include <stdbool.h>

#include "debuglog.h"
#include "deque.h"
#include "module.h"
#include "runtime.h"

struct sandbox_request {
	uint64_t         id;
	struct module *  module;
	char *           arguments;
	int              socket_descriptor;
	struct sockaddr *socket_address;
	uint64_t         request_arrival_timestamp; /* cycles */
	uint64_t         absolute_deadline;         /* cycles */

	/*
	 * Unitless estimate of the instantaneous fraction of system capacity required to run the request
	 * Calculated by estimated execution time (cycles) * runtime_admissions_granularity / relative deadline (cycles)
	 */
	uint64_t admissions_estimate;
};

DEQUE_PROTOTYPE(sandbox, struct sandbox_request *);

/**
 * Allocates a new Sandbox Request and places it on the Global Deque
 * @param module the module we want to request
 * @param arguments the arguments that we'll pass to the serverless function
 * @param socket_descriptor
 * @param socket_address
 * @param request_arrival_timestamp the timestamp of when we receives the request from the network (in cycles)
 * @return the new sandbox request
 */
static inline struct sandbox_request *
sandbox_request_allocate(struct module *module, char *arguments, int socket_descriptor,
                         const struct sockaddr *socket_address, uint64_t request_arrival_timestamp,
                         uint64_t admissions_estimate)
{
	struct sandbox_request *sandbox_request = (struct sandbox_request *)malloc(sizeof(struct sandbox_request));
	assert(sandbox_request);

	/* Sets the ID to the value before the increment */
	sandbox_request->id = atomic_fetch_add(&runtime_total_sandbox_requests, 1);
	assert(runtime_total_sandbox_requests + runtime_total_5XX_responses <= runtime_total_requests);

	sandbox_request->module                    = module;
	sandbox_request->arguments                 = arguments;
	sandbox_request->socket_descriptor         = socket_descriptor;
	sandbox_request->socket_address            = (struct sockaddr *)socket_address;
	sandbox_request->request_arrival_timestamp = request_arrival_timestamp;
	sandbox_request->absolute_deadline         = request_arrival_timestamp + module->relative_deadline;
	sandbox_request->admissions_estimate       = admissions_estimate;

#ifdef LOG_REQUEST_ALLOCATION
	debuglog("Allocating %lu of %s:%d\n", sandbox_request->request_arrival_timestamp, sandbox_request->module->name,
	         sandbox_request->module->port);
#endif
	return sandbox_request;
}
