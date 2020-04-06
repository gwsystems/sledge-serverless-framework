#include <sandbox_request_queue.h>
#include <priority_queue.h>

enum scheduling_policy
{
	FIFO,
	PS
};

enum scheduling_policy sandbox_request_queue_policy = FIFO;

// FIFO Globals
struct deque_sandbox *runtime_global_deque;
pthread_mutex_t       runtime_global_deque_mutex = PTHREAD_MUTEX_INITIALIZER;

// PS Globals
struct priority_queue sandbox_request_queue_ps;

static inline void
sandbox_request_queue_fifo_initialize(void)
{
	// Allocate and Initialize the global deque
	runtime_global_deque = (struct deque_sandbox *)malloc(sizeof(struct deque_sandbox));
	assert(runtime_global_deque);
	// Note: Below is a Macro
	deque_init_sandbox(runtime_global_deque, RUNTIME_MAX_SANDBOX_REQUEST_COUNT);
}

static inline void
sandbox_request_queue_ps_initialize(void)
{
	// TODO
}

void
sandbox_request_queue_initialize(void)
{
	switch (sandbox_request_queue_policy) {
	case FIFO:
		return sandbox_request_queue_fifo_initialize();
	case PS:
		return sandbox_request_queue_ps_initialize();
	}
}

/**
 * Pushes a sandbox request to the global deque
 * @param sandbox_request
 **/
static inline int
sandbox_request_queue_fifo_add(sandbox_request_t *sandbox_request)
{
	int return_code;

// TODO: Running the runtime and listener cores on a single shared core is untested
// We are unsure if the locking behavior is correct, so there may be deadlocks
#if NCORES == 1
	pthread_mutex_lock(&runtime_global_deque_mutex);
#endif
	return_code = deque_push_sandbox(runtime_global_deque, &sandbox_request);
#if NCORES == 1
	pthread_mutex_unlock(&runtime_global_deque_mutex);
#endif

	return return_code;
}

static inline int
sandbox_request_queue_ps_add(sandbox_request_t *sandbox_request)
{
	// TODO
	return 0;
}

/**
 * Pushes a sandbox request to the global deque
 * @param sandbox_request
 **/
int
sandbox_request_queue_add(sandbox_request_t *sandbox_request)
{
	switch (sandbox_request_queue_policy) {
	case FIFO:
		return sandbox_request_queue_fifo_add(sandbox_request);
	case PS:
		return sandbox_request_queue_ps_add(sandbox_request);
	}
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
sandbox_request_queue_fifo_remove(void)
{
	sandbox_request_t *sandbox_request = NULL;

#if NCORES == 1
	pthread_mutex_lock(&runtime_global_deque_mutex);
	return_code = deque_pop_sandbox(runtime_global_deque, sandbox_request);
	pthread_mutex_unlock(&runtime_global_deque_mutex);
#else
	int return_code = deque_steal_sandbox(runtime_global_deque, &sandbox_request);
#endif
	if (return_code) sandbox_request = NULL;
	return sandbox_request;
}

static inline sandbox_request_t *
sandbox_request_queue_ps_remove(void)
{
	sandbox_request_t *sandbox_request = NULL;
	// TODO
	return sandbox_request;
}

sandbox_request_t *
sandbox_request_queue_remove(void)
{
	switch (sandbox_request_queue_policy) {
	case FIFO:
		return sandbox_request_queue_fifo_remove();
	case PS:
		return sandbox_request_queue_ps_remove();
	}
}