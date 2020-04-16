// Something is not idempotent with this or some other include.
// If placed in Local Includes, error is triggered that memset was implicitly declared
#include <runtime.h>

/***************************
 * External Includes       *
 **************************/
#include <pthread.h>  // POSIX Threads
#include <signal.h>   // POSIX Signals
#include <sched.h>    // Wasmception. Included as submodule
#include <sys/mman.h> // Wasmception. Included as submodule
#include <uv.h>       // Libub

/***************************
 * Local Includes          *
 **************************/
#include <arch/context.h>
#include <current_sandbox.h>
#include <http_parser_settings.h>
#include <module.h>
#include <sandbox.h>
#include <sandbox_request.h>
// #include <sandbox_request_scheduler_fifo.h>
#include <sandbox_request_scheduler_ps.h>
#include <sandbox_run_queue.h>
#include <software_interrupt.h>
#include <types.h>

/***************************
 * Shared Process State    *
 **************************/

int                  runtime_epoll_file_descriptor;
http_parser_settings runtime_http_parser_settings;

/******************************************
 * Shared Process / Listener Thread Logic *
 ******************************************/

/**
 * Initialize runtime global state, mask signals, and init http parser
 */
void
runtime_initialize(void)
{
	runtime_epoll_file_descriptor = epoll_create1(0);
	assert(runtime_epoll_file_descriptor >= 0);

	// Allocate and Initialize the global deque
	// sandbox_request_scheduler_fifo_initialize();
	sandbox_request_scheduler_ps_initialize();

	// Mask Signals
	software_interrupt_mask_signal(SIGUSR1);
	software_interrupt_mask_signal(SIGALRM);

	// Initialize http_parser_settings global
	http_parser_settings_initialize(&runtime_http_parser_settings);
}

/********************************
 * Listener Thread Logic        *
 ********************************/

/**
 * @brief Execution Loop of the listener core, io_handles HTTP requests, allocates sandbox request objects, and pushes
 * the sandbox object to the global dequeue
 * @param dummy data pointer provided by pthreads API. Unused in this function
 * @return NULL
 *
 * Used Globals:
 * runtime_epoll_file_descriptor - the epoll file descriptor
 *
 */
void *
listener_thread_main(void *dummy)
{
	struct epoll_event *epoll_events   = (struct epoll_event *)malloc(LISTENER_THREAD_MAX_EPOLL_EVENTS
                                                                        * sizeof(struct epoll_event));
	int                 total_requests = 0;

	while (true) {
		int request_count = epoll_wait(runtime_epoll_file_descriptor, epoll_events,
		                               LISTENER_THREAD_MAX_EPOLL_EVENTS, -1);
		u64 start_time    = __getcycles();
		for (int i = 0; i < request_count; i++) {
			if (epoll_events[i].events & EPOLLERR) {
				perror("epoll_wait");
				assert(0);
			}

			struct sockaddr_in client_address;
			socklen_t          client_length = sizeof(client_address);
			struct module *    module        = (struct module *)epoll_events[i].data.ptr;
			assert(module);
			int es                = module->socket_descriptor;
			int socket_descriptor = accept(es, (struct sockaddr *)&client_address, &client_length);
			if (socket_descriptor < 0) {
				perror("accept");
				assert(0);
			}
			total_requests++;

			sandbox_request_t *sandbox_request =
			  sandbox_request_allocate(module, module->name, socket_descriptor,
			                           (const struct sockaddr *)&client_address, start_time);
			assert(sandbox_request);
			sandbox_request_scheduler_add(sandbox_request);
		}
	}

	free(epoll_events);

	return NULL;
}

/**
 * Initializes the listener thread, pinned to core 0, and starts to listen for requests
 */
void
listener_thread_initialize(void)
{
	cpu_set_t cs;

	CPU_ZERO(&cs);
	CPU_SET(LISTENER_THREAD_CORE_ID, &cs);

	pthread_t listener_thread;
	int       ret = pthread_create(&listener_thread, NULL, listener_thread_main, NULL);
	assert(ret == 0);
	ret = pthread_setaffinity_np(listener_thread, sizeof(cpu_set_t), &cs);
	assert(ret == 0);
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cs);
	assert(ret == 0);

	software_interrupt_initialize();
	software_interrupt_arm_timer();
}

/***************************
 * Worker Thread State     *
 **************************/

__thread static struct ps_list_head worker_thread_completion_queue;

// context pointer to switch to when this thread gets a SIGUSR1
__thread arch_context_t *worker_thread_next_context = NULL;

// context of the runtime thread before running sandboxes or to resume its "main".
__thread arch_context_t worker_thread_base_context;

// libuv i/o loop handle per sandboxing thread!
__thread uv_loop_t worker_thread_uvio_handle;

// Flag to signify if the thread is currently running callbacks in the libuv event loop
static __thread unsigned int worker_thread_is_in_callback;


/**************************************************
 * Worker Thread Logic
 *************************************************/

static inline void worker_thread_push_sandbox_to_run_queue(struct sandbox *sandbox);

/**
 * @brief Switches to the next sandbox, placing the current sandbox of the completion queue if in RETURNED state
 * @param next The Sandbox Context to switch to or NULL
 * @return void
 */
static inline void
worker_thread_switch_to_sandbox(struct sandbox *next_sandbox)
{
	arch_context_t *next_register_context = next_sandbox == NULL ? NULL : &next_sandbox->ctxt;
	software_interrupt_disable();
	struct sandbox *current_sandbox          = current_sandbox_get();
	arch_context_t *current_register_context = current_sandbox == NULL ? NULL : &current_sandbox->ctxt;
	current_sandbox_set(next_sandbox);
	// If the current sandbox we're switching from is in a RETURNED state, add to completion queue
	if (current_sandbox && current_sandbox->state == RETURNED)
		worker_thread_push_sandbox_to_completion_queue(current_sandbox);
	worker_thread_next_context = next_register_context;
	arch_context_switch(current_register_context, next_register_context);
	software_interrupt_enable();
}

/**
 * If this sandbox is blocked, mark it as runnable and add to the head of the thread-local runqueue
 * @param sandbox the sandbox to check and update if blocked
 **/
void
worker_thread_wakeup_sandbox(sandbox_t *sandbox)
{
	software_interrupt_disable();
	debuglog("[%p: %s]\n", sandbox, sandbox->module->name);
	if (sandbox->state != BLOCKED) goto done;
	assert(sandbox->state == BLOCKED);
	assert(ps_list_singleton_d(sandbox));
	sandbox->state = RUNNABLE;
	sandbox_run_queue_append(sandbox);
done:
	software_interrupt_enable();
}


/**
 * Mark the currently executing sandbox as blocked, remove it from the local runqueue, and pull the sandbox at the head
 *of the runqueue
 **/
void
worker_thread_block_current_sandbox(void)
{
	assert(worker_thread_is_in_callback == 0);
	software_interrupt_disable();
	struct sandbox *current_sandbox = current_sandbox_get();
	ps_list_rem_d(current_sandbox);
	current_sandbox->state       = BLOCKED;
	struct sandbox *next_sandbox = worker_thread_get_next_sandbox(0);
	debuglog("[%p: %next_sandbox, %p: %next_sandbox]\n", current_sandbox, current_sandbox->module->name,
	         next_sandbox, next_sandbox ? next_sandbox->module->name : "");
	software_interrupt_enable();
	worker_thread_switch_to_sandbox(next_sandbox);
}


/**
 * Execute I/O
 **/
void
worker_thread_process_io(void)
{
#ifdef USE_HTTP_UVIO
#ifdef USE_HTTP_SYNC
	// realistically, we're processing all async I/O on this core when a sandbox blocks on http processing, not
	// great! if there is a way (TODO), perhaps RUN_ONCE and check if your I/O is processed, if yes, return else do
	// async block!
	uv_run(worker_thread_get_libuv_handle(), UV_RUN_DEFAULT);
#else  /* USE_HTTP_SYNC */
	worker_thread_block_current_sandbox();
#endif /* USE_HTTP_UVIO */
#else
	assert(0);
	// it should not be called if not using uvio for http
#endif
}

/**
 * TODO: What is this doing?
 **/
void __attribute__((noinline)) __attribute__((noreturn)) worker_thread_sandbox_switch_preempt(void)
{
	pthread_kill(pthread_self(), SIGUSR1);

	assert(0); // should not get here..
	while (true)
		;
}

/**
 * Pulls up to 1..n sandbox requests, allocates them as sandboxes, sets them as runnable and places them on the local
 * runqueue, and then frees the sandbox requests The batch size pulled at once is set by SANDBOX_PULL_BATCH_SIZE
 * @return the number of sandbox requests pulled
 */
static inline int
worker_thread_pull_and_process_sandbox_requests(void)
{
	int total_sandboxes_pulled = 0;

	while (total_sandboxes_pulled < SANDBOX_PULL_BATCH_SIZE) {
		sandbox_request_t *sandbox_request;
		if ((sandbox_request = sandbox_request_scheduler_remove()) == NULL) break;
		// Actually allocate the sandbox for the requests that we've pulled
		struct sandbox *sandbox = sandbox_allocate(sandbox_request->module, sandbox_request->arguments,
		                                           sandbox_request->socket_descriptor,
		                                           sandbox_request->socket_address, sandbox_request->start_time,
		                                           sandbox_request->absolute_deadline);
		assert(sandbox);
		free(sandbox_request);
		// Set the sandbox as runnable and place on the local runqueue
		sandbox->state = RUNNABLE;
		sandbox_run_queue_append(sandbox);
		total_sandboxes_pulled++;
	}

	return total_sandboxes_pulled;
}

/**
 * Run all outstanding events in the local thread's libuv event loop
 **/
void
worker_thread_execute_libuv_event_loop(void)
{
	worker_thread_is_in_callback = 1;
	int n = uv_run(worker_thread_get_libuv_handle(), UV_RUN_NOWAIT), i = 0;
	while (n > 0) {
		n--;
		uv_run(worker_thread_get_libuv_handle(), UV_RUN_NOWAIT);
	}
	worker_thread_is_in_callback = 0;
}


/**
 * Removes the thread from the thread-local runqueue
 * @param sandbox sandbox
 **/
static inline void
worker_thread_pop_sandbox_from_run_queue(struct sandbox *sandbox)
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
worker_thread_get_next_sandbox(int in_interrupt)
{
	// If the thread local runqueue is empty and we're not running in the context of an interupt,
	// pull a fresh batch of sandbox requests from the global queue
	if (sandbox_run_queue_is_empty()) {
		// this is in an interrupt context, don't steal work here!
		if (in_interrupt) return NULL;
		if (worker_thread_pull_and_process_sandbox_requests() == 0) {
			// debuglog("[null: null]\n");
			return NULL;
		}
	}

	// Execute Round Robin Scheduling Logic
	// Grab the sandbox at the head of the thread local runqueue, add it to the end, and return it
	struct sandbox *sandbox = sandbox_run_queue_get_head();
	// We are assuming that any sandboxed in the RETURNED state should have been pulled from the local runqueue by
	// now!
	assert(sandbox->state != RETURNED);
	ps_list_rem_d(sandbox);
	sandbox_run_queue_append(sandbox);
	debuglog("[%p: %s]\n", sandbox, sandbox->module->name);
	return sandbox;
}

/**
 * Adds sandbox to the completion queue
 * @param sandbox
 **/
void
worker_thread_push_sandbox_to_completion_queue(struct sandbox *sandbox)
{
	assert(ps_list_singleton_d(sandbox));
	ps_list_head_append_d(&worker_thread_completion_queue, sandbox);
}


/**
 * @brief Pops n sandboxes from the thread local completion queue and then frees them
 * @param number_to_free The number of sandboxes to pop and free
 * @return void
 */
static inline void
worker_thread_pop_and_free_n_sandboxes_from_completion_queue(unsigned int number_to_free)
{
	for (int i = 0; i < number_to_free; i++) {
		if (ps_list_head_empty(&worker_thread_completion_queue)) break;
		struct sandbox *sandbox = ps_list_head_first_d(&worker_thread_completion_queue, struct sandbox);
		if (!sandbox) break;
		ps_list_rem_d(sandbox);
		sandbox_free(sandbox);
	}
}

/**
 * Tries to free a completed request, executes libuv callbacks, and then gets
 * and returns the standbox at the head of the thread-local runqueue
 * @return sandbox or NULL
 **/
static inline struct sandbox *
worker_thread_execute_runtime_maintenance_and_get_next_sandbox(void)
{
	assert(current_sandbox_get() == NULL);
	// Try to free one sandbox from the completion queue
	worker_thread_pop_and_free_n_sandboxes_from_completion_queue(1);
	// Execute libuv callbacks
	if (!worker_thread_is_in_callback) worker_thread_execute_libuv_event_loop();

	// Get and return the sandbox at the head of the thread local runqueue
	software_interrupt_disable();
	struct sandbox *sandbox = worker_thread_get_next_sandbox(0);
	software_interrupt_enable();
	assert(sandbox == NULL || sandbox->state == RUNNABLE);
	return sandbox;
}

/**
 * The entry function for sandbox worker threads
 * Initializes thread-local state, unmasks signals, sets up libuv loop and
 * @param return_code - argument provided by pthread API. We set to -1 on error
 **/
void *
worker_thread_main(void *return_code)
{
	arch_context_init(&worker_thread_base_context, 0, 0);

	sandbox_run_queue_initialize();
	ps_list_head_init(&worker_thread_completion_queue);
	software_interrupt_is_disabled = 0;
	worker_thread_next_context     = NULL;
#ifndef PREEMPT_DISABLE
	software_interrupt_unmask_signal(SIGALRM);
	software_interrupt_unmask_signal(SIGUSR1);
#endif
	uv_loop_init(&worker_thread_uvio_handle);
	worker_thread_is_in_callback = 0;

	while (true) {
		struct sandbox *sandbox = worker_thread_execute_runtime_maintenance_and_get_next_sandbox();
		while (sandbox) {
			worker_thread_switch_to_sandbox(sandbox);
			sandbox = worker_thread_execute_runtime_maintenance_and_get_next_sandbox();
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
worker_thread_exit_current_sandbox(void)
{
	struct sandbox *current_sandbox = current_sandbox_get();
	assert(current_sandbox);
	software_interrupt_disable();
	worker_thread_pop_sandbox_from_run_queue(current_sandbox);
	current_sandbox->state = RETURNED;

	struct sandbox *next_sandbox = worker_thread_get_next_sandbox(0);
	assert(next_sandbox != current_sandbox);
	software_interrupt_enable();
	// free resources from "main function execution", as stack still in use.
	// unmap linear memory only!
	munmap(current_sandbox->linear_memory_start, SBOX_MAX_MEM + PAGE_SIZE);
	worker_thread_switch_to_sandbox(next_sandbox);
}
