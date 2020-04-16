#ifndef SFRT_SANDBOX_REQUEST_H
#define SFRT_SANDBOX_REQUEST_H

#include "deque.h"
#include "types.h"
#include "runtime.h"

struct sandbox_request {
	struct module *  module;
	char *           arguments;
	int              socket_descriptor;
	struct sockaddr *socket_address;
	u64              start_time;        // cycles
	u64              absolute_deadline; // cycles
};
typedef struct sandbox_request sandbox_request_t;

DEQUE_PROTOTYPE(sandbox, sandbox_request_t *);

/**
 * Allocates a new Sandbox Request and places it on the Global Deque
 * @param module the module we want to request
 * @param arguments the arguments that we'll pass to the serverless function
 * @param socket_descriptor
 * @param socket_address
 * @param start_time the timestamp of when we receives the request from the network (in cycles)
 * @return the new sandbox request
 **/
static inline sandbox_request_t *
sandbox_request_allocate(struct module *module, char *arguments, int socket_descriptor,
                         const struct sockaddr *socket_address, u64 start_time)
{
	sandbox_request_t *sandbox_request = (sandbox_request_t *)malloc(sizeof(sandbox_request_t));
	assert(sandbox_request);
	sandbox_request->module            = module;
	sandbox_request->arguments         = arguments;
	sandbox_request->socket_descriptor = socket_descriptor;
	sandbox_request->socket_address    = (struct sockaddr *)socket_address;
	sandbox_request->start_time        = start_time;
	sandbox_request->absolute_deadline = start_time + module->relative_deadline_us * runtime_processor_speed_MHz;

	// TODO: Refactor to log file
	printf("Set absolute deadline of %lu\n", sandbox_request->absolute_deadline);

	debuglog("[%p: %s]\n", sandbox_request, sandbox_request->module->name);
	return sandbox_request;
}

#endif /* SFRT_SANDBOX_REQUEST_H */