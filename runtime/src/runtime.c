#include <runtime.h>
#include <sys/mman.h>
#include <types.h>
#include <sandbox.h>
#include <arch/context.h>
#include <module.h>
#include <signal.h>
#include <pthread.h>
#include <sched.h>
#include <softint.h>
#include <uv.h>
#include <http_api.h>

/***************************
 * Shared Process State    *
 **************************/

struct deque_sandbox *global_deque;
pthread_mutex_t       global_deque_mutex = PTHREAD_MUTEX_INITIALIZER;
int                   epoll_file_descriptor;

/***************************
 * Thread Local State      *
 **************************/

__thread static struct ps_list_head local_run_queue;
__thread static struct ps_list_head local_completion_queue;

// current sandbox that is active..
__thread sandbox_t *current_sandbox = NULL;

// context pointer to switch to when this thread gets a SIGUSR1
// TODO: Delete this? It doesn't seem to be used.
__thread arch_context_t *next_context = NULL;

// context of the runtime thread before running sandboxes or to resume its "main".
__thread arch_context_t base_context;

// libuv i/o loop handle per sandboxing thread!
__thread uv_loop_t uvio_handle;

// Flag to signify if the thread is currently running callbacks in the libuv event loop
static __thread unsigned int in_callback;

static inline void add_sandbox_to_local_run_queue(struct sandbox *sandbox);

/**
 * Pulls up to 1..n sandbox requests, allocates them as sandboxes, sets them as runnable and places them on the local
 * runqueue, and then frees the sandbox requests The batch size pulled at once is set by SBOX_PULL_MAX
 * @return the number of sandbox requests pulled
 */
static inline int
pull_sandbox_requests_from_global_runqueue(void)
{
	int total_sandboxes_pulled = 0;

	while (total_sandboxes_pulled < SBOX_PULL_MAX) {
		sandbox_request_t *sandbox_request;
		if ((sandbox_request = steal_sandbox_request_from_global_dequeue()) == NULL) break;
		// Actually allocate the sandbox for the requests that we've pulled
		struct sandbox *sandbox = allocate_sandbox(sandbox_request->module, sandbox_request->arguments,
		                                        sandbox_request->socket_descriptor, sandbox_request->socket_address,
		                                        sandbox_request->start_time);
		assert(sandbox);
		free(sandbox_request);
		// Set the sandbox as runnable and place on the local runqueue
		sandbox->state = RUNNABLE;
		add_sandbox_to_local_run_queue(sandbox);
		total_sandboxes_pulled++;
	}

	return total_sandboxes_pulled;
}

/**
 * Run all outstanding events in the libuv event loop
 **/
void
execute_libuv_event_loop(void)
{
	in_callback = 1;
	int n = uv_run(runtime_uvio(), UV_RUN_NOWAIT), i = 0;
	while (n > 0) {
		n--;
		uv_run(runtime_uvio(), UV_RUN_NOWAIT);
	}
	in_callback = 0;
}

/**
 * Append the sandbox to the local_run_queue
 * @param sandbox sandbox to add
 */
static inline void
add_sandbox_to_local_run_queue(struct sandbox *sandbox)
{
	assert(ps_list_singleton_d(sandbox));
		// fprintf(stderr, "(%d,%lu) %s: run %p, %s\n", sched_getcpu(), pthread_self(), __func__, s,
	// s->module->name);
	ps_list_head_append_d(&local_run_queue, sandbox);
}

/**
 * Removes the thread from the thread-local runqueue
 * TODO: is this correct?
 * @param sandbox sandbox
 **/
static inline void
remove_sandbox_from_local_run_queue(struct sandbox *sandbox)
{
	ps_list_rem_d(sandbox);
}

/**
 * Execute the sandbox at the head of the thread local runqueue
 * If the runqueue is empty, pull a fresh batch of sandbox requests, instantiate them, and then execute the new head
 * @param in_interrupt if this is getting called in the context of an interrupt
 * @return the sandbox to execute or NULL if none are available
 **/
struct sandbox *
get_next_sandbox_from_local_run_queue(int in_interrupt)
{
	// If the thread local runqueue is empty and we're not running in the context of an interupt, 
 	// pull a fresh batch of sandbox requests from the global queue
	if (ps_list_head_empty(&local_run_queue)) {
		// this is in an interrupt context, don't steal work here!
		if (in_interrupt) return NULL;
		if (pull_sandbox_requests_from_global_runqueue() == 0) {
			// debuglog("[null: null]\n");
			return NULL;
		}
	}

	// Execute Round Robin Scheduling Logic
	// Grab the sandbox at the head of the thread local runqueue, add it to the end, and return it
	struct sandbox *sandbox = ps_list_head_first_d(&local_run_queue, struct sandbox);
	// We are assuming that any sandboxed in the RETURNED state should have been pulled from the local runqueue by now!
	assert(sandbox->state != RETURNED);
	ps_list_rem_d(sandbox);
	ps_list_head_append_d(&local_run_queue, sandbox);
	debuglog("[%p: %s]\n", sandbox, sandbox->module->name);
	return sandbox;
}

/**
 * Adds sandbox to the completion queue
 * @param sandbox
 **/
void
add_sandbox_to_completion_queue(struct sandbox *sandbox)
{
	assert(ps_list_singleton_d(sandbox));
	ps_list_head_append_d(&local_completion_queue, sandbox);
}


/**
 * @brief Remove and free n sandboxes from the thread local completion queue
 * @param number_to_free The number of sandboxes to free
 * @return void
 */
static inline void
free_sandboxes_from_completion_queue(unsigned int number_to_free)
{
	for (int i = 0; i < number_to_free; i++) {
		if (ps_list_head_empty(&local_completion_queue)) break;
		struct sandbox *sandbox = ps_list_head_first_d(&local_completion_queue, struct sandbox);
		if (!sandbox) break;
		ps_list_rem_d(sandbox);
		free_sandbox(sandbox);
	}
}

/**
 * If this sandbox is blocked, mark it as runnable and add to the head of the thread-local runqueue
 * @param sandbox the sandbox to check and update if blocked
 **/
void
wakeup_sandbox(sandbox_t *sandbox)
{
	softint_disable();
	debuglog("[%p: %s]\n", sandbox, sandbox->module->name);
	if (sandbox->state != BLOCKED) goto done;
	assert(sandbox->state == BLOCKED);
	assert(ps_list_singleton_d(sandbox));
	sandbox->state = RUNNABLE;
	ps_list_head_append_d(&local_run_queue, sandbox);
done:
	softint_enable();
}


/**
 * Mark the currently executing sandbox as blocked, remove it from the local runqueue, and pull the sandbox at the head of the runqueue
 **/
void
block_current_sandbox(void)
{
	assert(in_callback == 0);
	softint_disable();
	struct sandbox *current_sandbox = get_current_sandbox();
	// TODO: What is this getting removed from again? the thread-local runqueue?
	ps_list_rem_d(current_sandbox);
	current_sandbox->state          = BLOCKED;
	struct sandbox *next_sandbox = get_next_sandbox_from_local_run_queue(0);
	debuglog("[%p: %next_sandbox, %p: %next_sandbox]\n", current_sandbox, current_sandbox->module->name, next_sandbox, next_sandbox ? next_sandbox->module->name : "");
	softint_enable();
	switch_to_sandbox(next_sandbox);
}


/**
 * TODO: What is this doing?
 **/
void
sandbox_block_http(void)
{
#ifdef USE_HTTP_UVIO
#ifdef USE_HTTP_SYNC
	// realistically, we're processing all async I/O on this core when a sandbox blocks on http processing, not
	// great! if there is a way (TODO), perhaps RUN_ONCE and check if your I/O is processed, if yes, return else do
	// async block!
	uv_run(runtime_uvio(), UV_RUN_DEFAULT);
#else /* USE_HTTP_SYNC */
	block_current_sandbox();
#endif /* USE_HTTP_UVIO */
#else
	assert(0);
	// it should not be called if not using uvio for http
#endif
}


/**
 * TODO: What is this doing?
 **/
void __attribute__((noinline)) __attribute__((noreturn)) sandbox_switch_preempt(void)
{
	pthread_kill(pthread_self(), SIGUSR1);

	assert(0); // should not get here..
	while (true)
		;
}

/**
 * Tries to free a completed request, executes libuv callbacks, and then gets 
 * and returns the standbox at the head of the thread-local runqueue
 * @return sandbox or NULL
 **/
struct sandbox *
sandbox_worker_single_loop(void)
{
	assert(get_current_sandbox() == NULL);
	// Try to free one sandbox from the completion queue
	free_sandboxes_from_completion_queue(1);
	// Execute libuv callbacks
	if (!in_callback) execute_libuv_event_loop();

	// Get and return the sandbox at the head of the thread local runqueue
	softint_disable();
	struct sandbox *sandbox = get_next_sandbox_from_local_run_queue(0);
	softint_enable();
	assert(sandbox == NULL || sandbox->state == RUNNABLE);
	return sandbox;
}

/**
 * The entry function for sandbox worker threads
 * Initializes thread-local state, unmasks signals, sets up libuv loop and 
 * @param return_code - argument provided by pthread API. We set to -1 on error
 **/
void *
sandbox_worker_main(void *return_code)
{
	arch_context_init(&base_context, 0, 0);

	ps_list_head_init(&local_run_queue);
	ps_list_head_init(&local_completion_queue);
	softint_off  = 0;
	next_context = NULL;
#ifndef PREEMPT_DISABLE
	softint_unmask(SIGALRM);
	softint_unmask(SIGUSR1);
#endif
	uv_loop_init(&uvio_handle);
	in_callback = 0;

	while (true) {
		struct sandbox *sandbox = sandbox_worker_single_loop();
		while (sandbox) {
			switch_to_sandbox(sandbox);
			sandbox = sandbox_worker_single_loop();
		}
	}

	*(int *)return_code = -1;
	pthread_exit(return_code);
}

/**
 * Called when the function in the sandbox exits
 * Removes the standbox from the thread-local runqueue, sets its state to RETURNED,
 * releases the linear memory, and then switches to the sandbox at the head of the runqueue
 * TODO: Why are we not adding to the completion queue here? That logic is commented out.
 **/
void
exit_current_sandbox(void)
{
	struct sandbox *current_sandbox = get_current_sandbox();
	assert(current_sandbox);
	softint_disable();
	remove_sandbox_from_local_run_queue(current_sandbox);
	current_sandbox->state = RETURNED;

	struct sandbox *next_sandbox = get_next_sandbox_from_local_run_queue(0);
	assert(next_sandbox != current_sandbox);
	softint_enable();
	// free resources from "main function execution", as stack still in use.
	// unmap linear memory only!
	munmap(current_sandbox->linear_memory_start, SBOX_MAX_MEM + PAGE_SIZE);
	// add_sandbox_to_completion_queue(current_sandbox);
	switch_to_sandbox(next_sandbox);
}

/**
 * @brief Execution Loop of the listener core, handles HTTP requests, allocates sandbox request objects, and pushes the
 * sandbox object to the global dequeue
 * @param dummy data pointer provided by pthreads API. Unused in this function
 * @return NULL
 *
 * Used Globals:
 * epoll_file_descriptor - the epoll file descriptor
 *
 */
void *
listener_thread_main(void *dummy)
{
	struct epoll_event *epoll_events   = (struct epoll_event *)malloc(EPOLL_MAX * sizeof(struct epoll_event));
	int                 total_requests = 0;

	while (true) {
		int request_count      = epoll_wait(epoll_file_descriptor, epoll_events, EPOLL_MAX, -1);
		u64 start_time = rdtsc();
		for (int i = 0; i < request_count; i++) {
			if (epoll_events[i].events & EPOLLERR) {
				perror("epoll_wait");
				assert(0);
			}

			struct sockaddr_in client_address;
			socklen_t          client_length = sizeof(client_address);
			struct module *    module     = (struct module *)epoll_events[i].data.ptr;
			assert(module);
			int es = module->socket_descriptor;
			int socket_descriptor  = accept(es, (struct sockaddr *)&client_address, &client_length);
			if (socket_descriptor < 0) {
				perror("accept");
				assert(0);
			}
			total_requests++;
			printf("Received Request %d at %lu\n", total_requests, start_time);

			sandbox_request_t *sandbox_request = allocate_sandbox_request(
				module, 
				module->name, 
				socket_descriptor, 
				(const struct sockaddr *)&client_address,
			    start_time);
			assert(sandbox_request);

			// TODO: Refactor allocate_sandbox_request to not add to global request queue and do this here
		}
	}

	free(epoll_events);

	return NULL;
}

/**
 * Initialize runtime global state, mask signals, and init http parser
 */
void
initialize_runtime(void)
{
	epoll_file_descriptor = epoll_create1(0);
	assert(epoll_file_descriptor >= 0);

	// Allocate and Initialize the global deque
	global_deque = (struct deque_sandbox *)malloc(sizeof(struct deque_sandbox));
	assert(global_deque);
	// Note: Below is a Macro
	deque_init_sandbox(global_deque, SBOX_MAX_REQS);

	// Mask Signals
	softint_mask(SIGUSR1);
	softint_mask(SIGALRM);

	// Initialize http-parser
	http_init();
}

/**
 * Initializes the listener thread, pinned to core 0, and starts to listen for requests
 */
void
initialize_listener_thread(void)
{
	cpu_set_t cs;

	CPU_ZERO(&cs);
	CPU_SET(MOD_REQ_CORE, &cs);

	pthread_t listener_thread;
	int       ret = pthread_create(&listener_thread, NULL, listener_thread_main, NULL);
	assert(ret == 0);
	ret = pthread_setaffinity_np(listener_thread, sizeof(cpu_set_t), &cs);
	assert(ret == 0);
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cs);
	assert(ret == 0);

	softint_init();
	softint_timer_arm();
}
