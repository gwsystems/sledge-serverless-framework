#include "global_request_scheduler.h"

static struct deque_sandbox *global_request_scheduler_deque;
static pthread_mutex_t       global_request_scheduler_deque_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Pushes a sandbox request to the global deque
 * @param sandbox_request
 * @returns pointer to request if added. NULL otherwise
 */
static sandbox_request_t *
global_request_scheduler_deque_add(void *sandbox_request_raw)
{
	sandbox_request_t *sandbox_request = (sandbox_request_t *)sandbox_request_raw;
	int                return_code     = 1;

/* TODO: Running the runtime and listener cores on a single shared core is untested
We are unsure if the locking behavior is correct, so there may be deadlocks */
#if NCORES == 1
	pthread_mutex_lock(&global_request_scheduler_deque_mutex);
#endif
	return_code = deque_push_sandbox(global_request_scheduler_deque, &sandbox_request);
#if NCORES == 1
	pthread_mutex_unlock(&global_request_scheduler_deque_mutex);
#endif
	if (return_code != 0) return NULL;
	return sandbox_request_raw;
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
 * @returns 0 if successfully returned a sandbox request, -1 if empty, -2 if atomic instruction unsuccessful
 */
static int
global_request_scheduler_deque_remove(sandbox_request_t **removed_sandbox_request)
{
	int return_code;
#if NCORES == 1
	pthread_mutex_lock(&global_request_scheduler_deque_mutex);
	return_code = deque_pop_sandbox(global_request_scheduler_deque, *removed_sandbox_request);
	pthread_mutex_unlock(&global_request_scheduler_deque_mutex);
#else
	return_code = deque_steal_sandbox(global_request_scheduler_deque, removed_sandbox_request);
	/* The Deque uses different return codes other than 0, so map here */
	if (return_code == -2) {
		return_code = -1;
	} else if (return_code == -11) {
		return_code = -2;
	}

#endif
	return return_code;
}

void
global_request_scheduler_deque_initialize()
{
	/* Allocate and Initialize the global deque */
	global_request_scheduler_deque = (struct deque_sandbox *)malloc(sizeof(struct deque_sandbox));
	assert(global_request_scheduler_deque);
	/* Note: Below is a Macro */
	deque_init_sandbox(global_request_scheduler_deque, RUNTIME_MAX_SANDBOX_REQUEST_COUNT);

	/* Register Function Pointers for Abstract Scheduling API */
	struct global_request_scheduler_config config = { .add_fn    = global_request_scheduler_deque_add,
		                                          .remove_fn = global_request_scheduler_deque_remove };

	global_request_scheduler_initialize(&config);
}