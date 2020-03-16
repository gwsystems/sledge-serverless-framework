#ifndef SFRT_SANDBOX_REQUEST_H
#define SFRT_SANDBOX_REQUEST_H

#include "types.h"
#include "deque.h"

extern struct deque_sandbox *global_deque;
extern pthread_mutex_t       global_deque_mutex;

struct sandbox_request {
	struct module *  module;
	char *           arguments;
	int              socket_descriptor;
	struct sockaddr *socket_address;
	u64              start_time; // cycles
};
typedef struct sandbox_request sandbox_request_t;

DEQUE_PROTOTYPE(sandbox, sandbox_request_t *);

/**
 * Pushes a sandbox request to the global deque
 * @param sandbox_request 
 **/
static inline int
sandbox_request__add_to_global_dequeue(sandbox_request_t *sandbox_request)
{
	int return_code;

#if NCORES == 1
	pthread_mutex_lock(&global_deque_mutex);
#endif
	return_code = deque_push_sandbox(global_deque, &sandbox_request);
#if NCORES == 1
	pthread_mutex_unlock(&global_deque_mutex);
#endif

	return return_code;
}

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
sandbox_request__allocate(struct module *module, char *arguments, int socket_descriptor, const struct sockaddr *socket_address, u64 start_time)
{
	sandbox_request_t *sandbox_request = (sandbox_request_t *)malloc(sizeof(sandbox_request_t));
	assert(sandbox_request);
	sandbox_request->module     = module;
	sandbox_request->arguments       = arguments;
	sandbox_request->socket_descriptor       = socket_descriptor;
	sandbox_request->socket_address       = (struct sockaddr *)socket_address;
	sandbox_request->start_time = start_time;

	debuglog("[%p: %s]\n", sandbox_request, sandbox_request->module->name);
	sandbox_request__add_to_global_dequeue(sandbox_request);
	return sandbox_request;
}

/**
 * Pops a sandbox request from the global deque
 * @param sandbox_request the pointer which we want to set to the sandbox request
 **/
static inline int
sandbox_request__pop_from_global_dequeue(sandbox_request_t **sandbox_request)
{
	int return_code;

#if NCORES == 1
	pthread_mutex_lock(&global_deque_mutex);
#endif
	return_code = deque_pop_sandbox(global_deque, sandbox_request);
#if NCORES == 1
	pthread_mutex_unlock(&global_deque_mutex);
#endif
	return return_code;
}

/**
 * TODO: What does this do?
 * @returns A Sandbox Request or NULL
 **/
static inline sandbox_request_t *
sandbox_request__steal_from_global_dequeue(void)
{
	sandbox_request_t *sandbox_request = NULL;

#if NCORES == 1
	sandbox_request__pop_from_global_dequeue(&sandbox_request);
#else
	int r = deque_steal_sandbox(global_deque, &sandbox_request);
	if (r) sandbox_request = NULL;
#endif

	return sandbox_request;
}

#endif /* SFRT_SANDBOX_REQUEST_H */