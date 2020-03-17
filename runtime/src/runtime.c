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
#include <util.h>
#include <http/http_parser_settings.h>
#include <current_sandbox.h>

#include "sandbox_request.h"

/***************************
 * Shared Process State    *
 **************************/

struct deque_sandbox *runtime__global_deque;
pthread_mutex_t       runtime__global_deque_mutex = PTHREAD_MUTEX_INITIALIZER;
int                   runtime__epoll_file_descriptor;
http_parser_settings  runtime__http_parser_settings;

/******************************************
 * Shared Process / Listener Thread Logic *
 ******************************************/

/**
 * Initialize runtime global state, mask signals, and init http parser
 */
void
runtime__initialize(void)
{
	runtime__epoll_file_descriptor = epoll_create1(0);
	assert(runtime__epoll_file_descriptor >= 0);

	// Allocate and Initialize the global deque
	runtime__global_deque = (struct deque_sandbox *)malloc(sizeof(struct deque_sandbox));
	assert(runtime__global_deque);
	// Note: Below is a Macro
	deque_init_sandbox(runtime__global_deque, SBOX_MAX_REQS);

	// Mask Signals
	softint__mask(SIGUSR1);
	softint__mask(SIGALRM);

	// Initialize http_parser_settings global
	http_parser_settings__initialize(&runtime__http_parser_settings);
}

/********************************
 * Listener Thread Logic        *
 ********************************/

/**
 * @brief Execution Loop of the listener core, handles HTTP requests, allocates sandbox request objects, and pushes the
 * sandbox object to the global dequeue
 * @param dummy data pointer provided by pthreads API. Unused in this function
 * @return NULL
 *
 * Used Globals:
 * runtime__epoll_file_descriptor - the epoll file descriptor
 *
 */
void *
listener_thread__main(void *dummy)
{
	struct epoll_event *epoll_events   = (struct epoll_event *)malloc(EPOLL_MAX * sizeof(struct epoll_event));
	int                 total_requests = 0;

	while (true) {
		int request_count      = epoll_wait(runtime__epoll_file_descriptor, epoll_events, EPOLL_MAX, -1);
		u64 start_time = util__rdtsc();
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

			sandbox_request_t *sandbox_request = sandbox_request__allocate(
				module, 
				module->name, 
				socket_descriptor, 
				(const struct sockaddr *)&client_address,
			    start_time);
			assert(sandbox_request);

			// TODO: Refactor sandbox_request__allocate to not add to global request queue and do this here
		}
	}

	free(epoll_events);

	return NULL;
}

/**
 * Initializes the listener thread, pinned to core 0, and starts to listen for requests
 */
void
listener_thread__initialize(void)
{
	cpu_set_t cs;

	CPU_ZERO(&cs);
	CPU_SET(MOD_REQ_CORE, &cs);

	pthread_t listener_thread;
	int       ret = pthread_create(&listener_thread, NULL, listener_thread__main, NULL);
	assert(ret == 0);
	ret = pthread_setaffinity_np(listener_thread, sizeof(cpu_set_t), &cs);
	assert(ret == 0);
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cs);
	assert(ret == 0);

	softint__initialize();
	softint__arm_timer();
}

/***************************
 * Worker Thread State     *
 **************************/

__thread static struct ps_list_head worker_thread__run_queue;
__thread static struct ps_list_head worker_thread__completion_queue;

// current sandbox that is active..
__thread sandbox_t *worker_thread__current_sandbox = NULL;

// context pointer to switch to when this thread gets a SIGUSR1
__thread arch_context_t *worker_thread__next_context = NULL;

// context of the runtime thread before running sandboxes or to resume its "main".
__thread arch_context_t worker_thread__base_context;

// libuv i/o loop handle per sandboxing thread!
__thread uv_loop_t worker_thread__uvio_handle;

// Flag to signify if the thread is currently running callbacks in the libuv event loop
static __thread unsigned int worker_thread__is_in_callback;


/**************************************************
 * Worker Thread Logic
 *************************************************/

static inline void worker_thread__run_queue__add_sandbox(struct sandbox *sandbox);

/**
 * @brief Switches to the next sandbox, placing the current sandbox of the completion queue if in RETURNED state
 * @param next The Sandbox Context to switch to or NULL
 * @return void
 */
static inline void
worker_thread__switch_to_sandbox(struct sandbox *next_sandbox)
{
	arch_context_t *next_register_context = next_sandbox == NULL ? NULL : &next_sandbox->ctxt;
	softint__disable();
	struct sandbox *current_sandbox          = current_sandbox__get();
	arch_context_t *current_register_context = current_sandbox == NULL ? NULL : &current_sandbox->ctxt;
	current_sandbox__set(next_sandbox);
	// If the current sandbox we're switching from is in a RETURNED state, add to completion queue
	if (current_sandbox && current_sandbox->state == RETURNED) worker_thread__completion_queue__add_sandbox(current_sandbox);
	worker_thread__next_context = next_register_context;
	arch_context_switch(current_register_context, next_register_context);
	softint__enable();
}

/**
 * If this sandbox is blocked, mark it as runnable and add to the head of the thread-local runqueue
 * @param sandbox the sandbox to check and update if blocked
 **/
void
worker_thread__wakeup_sandbox(sandbox_t *sandbox)
{
	softint__disable();
	debuglog("[%p: %s]\n", sandbox, sandbox->module->name);
	if (sandbox->state != BLOCKED) goto done;
	assert(sandbox->state == BLOCKED);
	assert(ps_list_singleton_d(sandbox));
	sandbox->state = RUNNABLE;
	ps_list_head_append_d(&worker_thread__run_queue, sandbox);
done:
	softint__enable();
}


/**
 * Mark the currently executing sandbox as blocked, remove it from the local runqueue, and pull the sandbox at the head of the runqueue
 **/
void
worker_thread__block_current_sandbox(void)
{
	assert(worker_thread__is_in_callback == 0);
	softint__disable();
	struct sandbox *current_sandbox = current_sandbox__get();
	ps_list_rem_d(current_sandbox);
	current_sandbox->state          = BLOCKED;
	struct sandbox *next_sandbox = worker_thread__get_next_sandbox(0);
	debuglog("[%p: %next_sandbox, %p: %next_sandbox]\n", current_sandbox, current_sandbox->module->name, next_sandbox, next_sandbox ? next_sandbox->module->name : "");
	softint__enable();
	worker_thread__switch_to_sandbox(next_sandbox);
}


/**
 * Execute I/O
 **/
void
worker_thread__process_io(void)
{
#ifdef USE_HTTP_UVIO
#ifdef USE_HTTP_SYNC
	// realistically, we're processing all async I/O on this core when a sandbox blocks on http processing, not
	// great! if there is a way (TODO), perhaps RUN_ONCE and check if your I/O is processed, if yes, return else do
	// async block!
	uv_run(get_thread_libuv_handle(), UV_RUN_DEFAULT);
#else /* USE_HTTP_SYNC */
	worker_thread__block_current_sandbox();
#endif /* USE_HTTP_UVIO */
#else
	assert(0);
	// it should not be called if not using uvio for http
#endif
}

/**
 * TODO: What is this doing?
 **/
void __attribute__((noinline)) __attribute__((noreturn)) worker_thread__sandbox_switch_preempt(void)
{
	pthread_kill(pthread_self(), SIGUSR1);

	assert(0); // should not get here..
	while (true)
		;
}

/**
 * Pulls up to 1..n sandbox requests, allocates them as sandboxes, sets them as runnable and places them on the local
 * runqueue, and then frees the sandbox requests The batch size pulled at once is set by SBOX_PULL_MAX
 * @return the number of sandbox requests pulled
 */
static inline int
worker_thread__pull_and_process_sandbox_requests(void)
{
	int total_sandboxes_pulled = 0;

	while (total_sandboxes_pulled < SBOX_PULL_MAX) {
		sandbox_request_t *sandbox_request;
		if ((sandbox_request = sandbox_request__steal_from_global_dequeue()) == NULL) break;
		// Actually allocate the sandbox for the requests that we've pulled
		struct sandbox *sandbox = sandbox__allocate(sandbox_request->module, sandbox_request->arguments,
		                                        sandbox_request->socket_descriptor, sandbox_request->socket_address,
		                                        sandbox_request->start_time);
		assert(sandbox);
		free(sandbox_request);
		// Set the sandbox as runnable and place on the local runqueue
		sandbox->state = RUNNABLE;
		worker_thread__run_queue__add_sandbox(sandbox);
		total_sandboxes_pulled++;
	}

	return total_sandboxes_pulled;
}

/**
 * Run all outstanding events in the local thread's libuv event loop
 **/
void
worker_thread__execute_libuv_event_loop(void)
{
	worker_thread__is_in_callback = 1;
	int n = uv_run(get_thread_libuv_handle(), UV_RUN_NOWAIT), i = 0;
	while (n > 0) {
		n--;
		uv_run(get_thread_libuv_handle(), UV_RUN_NOWAIT);
	}
	worker_thread__is_in_callback = 0;
}

/**
 * Append the sandbox to the worker_thread__run_queue
 * @param sandbox sandbox to add
 */
static inline void
worker_thread__run_queue__add_sandbox(struct sandbox *sandbox)
{
	assert(ps_list_singleton_d(sandbox));
		// fprintf(stderr, "(%d,%lu) %s: run %p, %s\n", sched_getcpu(), pthread_self(), __func__, s,
	// s->module->name);
	ps_list_head_append_d(&worker_thread__run_queue, sandbox);
}

/**
 * Removes the thread from the thread-local runqueue
 * @param sandbox sandbox
 **/
static inline void
worker_thread__run_queue__remove_sandbox(struct sandbox *sandbox)
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
worker_thread__get_next_sandbox(int in_interrupt)
{
	// If the thread local runqueue is empty and we're not running in the context of an interupt, 
 	// pull a fresh batch of sandbox requests from the global queue
	if (ps_list_head_empty(&worker_thread__run_queue)) {
		// this is in an interrupt context, don't steal work here!
		if (in_interrupt) return NULL;
		if (worker_thread__pull_and_process_sandbox_requests() == 0) {
			// debuglog("[null: null]\n");
			return NULL;
		}
	}

	// Execute Round Robin Scheduling Logic
	// Grab the sandbox at the head of the thread local runqueue, add it to the end, and return it
	struct sandbox *sandbox = ps_list_head_first_d(&worker_thread__run_queue, struct sandbox);
	// We are assuming that any sandboxed in the RETURNED state should have been pulled from the local runqueue by now!
	assert(sandbox->state != RETURNED);
	ps_list_rem_d(sandbox);
	ps_list_head_append_d(&worker_thread__run_queue, sandbox);
	debuglog("[%p: %s]\n", sandbox, sandbox->module->name);
	return sandbox;
}

/**
 * Adds sandbox to the completion queue
 * @param sandbox
 **/
void
worker_thread__completion_queue__add_sandbox(struct sandbox *sandbox)
{
	assert(ps_list_singleton_d(sandbox));
	ps_list_head_append_d(&worker_thread__completion_queue, sandbox);
}


/**
 * @brief Remove and free n sandboxes from the thread local completion queue
 * @param number_to_free The number of sandboxes to free
 * @return void
 */
static inline void
worker_thread__completion_queue__free_sandboxes(unsigned int number_to_free)
{
	for (int i = 0; i < number_to_free; i++) {
		if (ps_list_head_empty(&worker_thread__completion_queue)) break;
		struct sandbox *sandbox = ps_list_head_first_d(&worker_thread__completion_queue, struct sandbox);
		if (!sandbox) break;
		ps_list_rem_d(sandbox);
		sandbox__free(sandbox);
	}
}

/**
 * Tries to free a completed request, executes libuv callbacks, and then gets 
 * and returns the standbox at the head of the thread-local runqueue
 * @return sandbox or NULL
 **/
struct sandbox *
worker_thread__single_loop(void)
{
	assert(current_sandbox__get() == NULL);
	// Try to free one sandbox from the completion queue
	worker_thread__completion_queue__free_sandboxes(1);
	// Execute libuv callbacks
	if (!worker_thread__is_in_callback) worker_thread__execute_libuv_event_loop();

	// Get and return the sandbox at the head of the thread local runqueue
	softint__disable();
	struct sandbox *sandbox = worker_thread__get_next_sandbox(0);
	softint__enable();
	assert(sandbox == NULL || sandbox->state == RUNNABLE);
	return sandbox;
}

/**
 * The entry function for sandbox worker threads
 * Initializes thread-local state, unmasks signals, sets up libuv loop and 
 * @param return_code - argument provided by pthread API. We set to -1 on error
 **/
void *
worker_thread__main(void *return_code)
{
	arch_context_init(&worker_thread__base_context, 0, 0);

	ps_list_head_init(&worker_thread__run_queue);
	ps_list_head_init(&worker_thread__completion_queue);
	softint__is_disabled  = 0;
	worker_thread__next_context = NULL;
#ifndef PREEMPT_DISABLE
	softint__unmask(SIGALRM);
	softint__unmask(SIGUSR1);
#endif
	uv_loop_init(&worker_thread__uvio_handle);
	worker_thread__is_in_callback = 0;

	while (true) {
		struct sandbox *sandbox = worker_thread__single_loop();
		while (sandbox) {
			worker_thread__switch_to_sandbox(sandbox);
			sandbox = worker_thread__single_loop();
		}
	}

	*(int *)return_code = -1;
	pthread_exit(return_code);
}

/**
 * Called when the function in the sandbox exits
 * Removes the standbox from the thread-local runqueue, sets its state to RETURNED,
 * releases the linear memory, and then switches to the sandbox at the head of the runqueue
 * TODO: Consider moving this to a future current_sandbox file. This has thus far proven difficult to move
 **/
void
worker_thread__current_sandbox__exit(void)
{
	struct sandbox *current_sandbox = current_sandbox__get();
	assert(current_sandbox);
	softint__disable();
	worker_thread__run_queue__remove_sandbox(current_sandbox);
	current_sandbox->state = RETURNED;

	struct sandbox *next_sandbox = worker_thread__get_next_sandbox(0);
	assert(next_sandbox != current_sandbox);
	softint__enable();
	// free resources from "main function execution", as stack still in use.
	// unmap linear memory only!
	munmap(current_sandbox->linear_memory_start, SBOX_MAX_MEM + PAGE_SIZE);
	worker_thread__switch_to_sandbox(next_sandbox);
}