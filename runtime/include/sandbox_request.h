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
	u64              start_time; // cycles
};
typedef struct sandbox_request sandbox_request_t;

DEQUE_PROTOTYPE(sandbox, sandbox_request_t *);

/**
 * Pushes a sandbox request to the global deque
 * @param sandbox_request
 **/
static inline int
sandbox_request__push_to_dequeue(sandbox_request_t *sandbox_request)
{
	int return_code;

// TODO: Running the runtime and listener cores on a single shared core is untested
// We are unsure if the locking behavior is correct, so there may be deadlocks
#if NCORES == 1
	pthread_mutex_lock(&runtime__global_deque_mutex);
#endif
	return_code = deque_push_sandbox(runtime__global_deque, &sandbox_request);
#if NCORES == 1
	pthread_mutex_unlock(&runtime__global_deque_mutex);
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
sandbox_request__allocate(struct module *module, char *arguments, int socket_descriptor,
                          const struct sockaddr *socket_address, u64 start_time)
{
	sandbox_request_t *sandbox_request = (sandbox_request_t *)malloc(sizeof(sandbox_request_t));
	assert(sandbox_request);
	sandbox_request->module            = module;
	sandbox_request->arguments         = arguments;
	sandbox_request->socket_descriptor = socket_descriptor;
	sandbox_request->socket_address    = (struct sockaddr *)socket_address;
	sandbox_request->start_time        = start_time;

	debuglog("[%p: %s]\n", sandbox_request, sandbox_request->module->name);
	sandbox_request__push_to_dequeue(sandbox_request);
	return sandbox_request;
}

/**
 * Pops a sandbox request from the global deque
 * @param sandbox_request the pointer which we want to set to the sandbox request
 **/
static inline int
sandbox_request__pop_from_dequeue(sandbox_request_t **sandbox_request)
{
	int return_code;

// TODO: Running the runtime and listener cores on a single shared core is untested
// We are unsure if the locking behavior is correct, so there may be deadlocks
#if NCORES == 1
	pthread_mutex_lock(&runtime__global_deque_mutex);
#endif
	return_code = deque_pop_sandbox(runtime__global_deque, sandbox_request);
#if NCORES == 1
	pthread_mutex_unlock(&runtime__global_deque_mutex);
#endif
	return return_code;
}

/**
 * Stealing from the dequeue is a lock-free, cross-core "pop", which removes the element from the end opposite to
 * "pop". Because the producer and consumer (the core stealine the sandbox request) modify different ends,
 * no locks are required, and coordination is achieved by instead retrying on inconsistent indices.
 *
 * Relevant Read: https://www.dre.vanderbilt.edu/~schmidt/PDF/work-stealing-dequeue.pdf
 *
 * TODO: Notice the mutex_lock for NCORES == 1 in both push/pop functions and steal calling 'pop' for NCORES == 1.
 * Ideally you don't call steal for same core consumption but I just made the steal API wrap that logic. Which is
 * perhaps not good. We might just add the #if in the scheduling code which should explicitly call "pop" for single core
 * and add an assert in "steal" function for NCORES == 1.
 *
 * @returns A Sandbox Request or NULL
 **/
static inline sandbox_request_t *
sandbox_request__steal_from_dequeue(void)
{
	sandbox_request_t *sandbox_request = NULL;

#if NCORES == 1
	sandbox_request__pop_from_dequeue(&sandbox_request);
#else
	int r = deque_steal_sandbox(runtime__global_deque, &sandbox_request);
	if (r) sandbox_request = NULL;
#endif

	return sandbox_request;
}

#endif /* SFRT_SANDBOX_REQUEST_H */