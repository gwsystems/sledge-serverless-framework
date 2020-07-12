#pragma once

#include <stdbool.h>

#include "deque.h"
#include "module.h"
#include "runtime.h"
#include "types.h"

extern float runtime_processor_speed_MHz;

struct sandbox_request {
	struct module *  module;
	char *           arguments;
	int              socket_descriptor;
	struct sockaddr *socket_address;
	uint64_t         request_timestamp; /* cycles */
	uint64_t         absolute_deadline; /* cycles */
};

DEQUE_PROTOTYPE(sandbox, struct sandbox_request *);

/**
 * Allocates a new Sandbox Request and places it on the Global Deque
 * @param module the module we want to request
 * @param arguments the arguments that we'll pass to the serverless function
 * @param socket_descriptor
 * @param socket_address
 * @param start_time the timestamp of when we receives the request from the network (in cycles)
 * @return the new sandbox request
 */
static inline struct sandbox_request *
sandbox_request_allocate(struct module *module, char *arguments, int socket_descriptor,
                         const struct sockaddr *socket_address, uint64_t request_timestamp)
{
	struct sandbox_request *sandbox_request = (struct sandbox_request *)malloc(sizeof(struct sandbox_request));
	assert(sandbox_request);
	sandbox_request->module            = module;
	sandbox_request->arguments         = arguments;
	sandbox_request->socket_descriptor = socket_descriptor;
	sandbox_request->socket_address    = (struct sockaddr *)socket_address;
	sandbox_request->request_timestamp = request_timestamp;
	sandbox_request->absolute_deadline = request_timestamp
	                                     + module->relative_deadline_us * runtime_processor_speed_MHz;

	debuglog("[%p: %s]\n", sandbox_request, sandbox_request->module->name);
	return sandbox_request;
}
