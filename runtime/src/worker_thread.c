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
#include <uv.h>       // Libuv

/***************************
 * Local Includes          *
 **************************/
#include <current_sandbox.h>
#include <sandbox_completion_queue.h>
#include <sandbox_request_scheduler.h>
#include <sandbox_run_queue.h>
#include <sandbox_run_queue_fifo.h>
#include <sandbox_run_queue_ps.h>
#include <types.h>
#include <worker_thread.h>

/***************************
 * Worker Thread State     *
 **************************/

// context pointer used to store and restore a preempted sandbox. SIGUSR1
__thread struct arch_context *worker_thread_next_context = NULL;

// context of the runtime thread before running sandboxes or to resume its "main".
__thread struct arch_context worker_thread_base_context;

// libuv i/o loop handle per sandboxing thread!
__thread uv_loop_t worker_thread_uvio_handle;

// Flag to signify if the thread is currently running callbacks in the libuv event loop
static __thread bool worker_thread_is_in_libuv_event_loop;

/**************************************************
 * Worker Thread Logic
 *************************************************/

/**
 * Conditionally triggers appropriate state changes for exiting sandboxes
 * @param exiting_sandbox - The sandbox that ran to completion
 **/
static inline void
worker_thread_transition_exiting_sandbox(struct sandbox *exiting_sandbox)
{
	assert(exiting_sandbox != NULL);

	switch (exiting_sandbox->state) {
	case SANDBOX_RETURNED:
		// We draw a distinction between RETURNED and COMPLETED because a sandbox cannot add itself to the
		// completion queue
		sandbox_set_as_complete(exiting_sandbox);
		break;
	case SANDBOX_ERROR:
		// Terminal State, so just break
		break;
	default:
		printf("Cooperatively switching from a sandbox in a non-terminal %s state\n",
		       sandbox_state_stringify(exiting_sandbox->state));
		assert(0);
	}
}

/**
 * Switches to the next sandbox, placing the current sandbox on the completion queue if in RETURNED state
 * TODO: Confirm that this can gracefully resume sandboxes in a PREEMPTED state
 * @param next_sandbox The Sandbox Context to switch to
 */
static inline void
worker_thread_switch_to_sandbox(struct sandbox *next_sandbox)
{
	assert(next_sandbox != NULL);
	struct arch_context *next_context = &next_sandbox->ctxt;

	software_interrupt_disable();

	// If we are switching to a new sandbox that we haven't preempted before, buffer the context
	// I don't yet fully understand why.
	if (next_sandbox->state == SANDBOX_RUNNABLE) worker_thread_next_context = next_context;

	struct sandbox *current_sandbox = current_sandbox_get();

	if (current_sandbox == NULL) {
		// Switching from "Base Context"
		sandbox_set_as_running(next_sandbox, NULL);

		printf("Thread %lu | Switching from Base Context to Sandbox %lu\n", pthread_self(),
		       next_sandbox->allocation_timestamp);

		arch_context_switch(NULL, next_context);
	} else {
		// Switching between sandboxes
		assert(next_sandbox != current_sandbox);

		worker_thread_transition_exiting_sandbox(current_sandbox);

		sandbox_set_as_running(next_sandbox, NULL);

		printf("Thread %lu | Switching from Sandbox %lu to Sandbox %lu\n", pthread_self(),
		       current_sandbox->allocation_timestamp, next_sandbox->allocation_timestamp);

		arch_context_switch(&current_sandbox->ctxt, next_context);
	}

	software_interrupt_enable();
}

/**
 * @brief Switches to the base context, placing the current sandbox on the completion queue if in RETURNED state
 */
static inline void
worker_thread_switch_to_base_context()
{
	// I'm still figuring this global out. I believe this should always have been cleared by this point
	assert(worker_thread_next_context == NULL);

	software_interrupt_disable();

	struct sandbox *current_sandbox = current_sandbox_get();
	assert(current_sandbox != NULL);
	assert(&current_sandbox->ctxt != &worker_thread_base_context);

	worker_thread_transition_exiting_sandbox(current_sandbox);

	current_sandbox_set(NULL);
	printf("Thread %lu | Switching from Sandbox %lu to Base Context\n", pthread_self(),
	       current_sandbox->allocation_timestamp);
	arch_context_switch(&current_sandbox->ctxt, &worker_thread_base_context);
	software_interrupt_enable();
}

/**
 * Mark a blocked sandbox as runnable and add it to the runqueue
 * @param sandbox the sandbox to check and update if blocked
 **/
void
worker_thread_wakeup_sandbox(sandbox_t *sandbox)
{
	software_interrupt_disable();
	// debuglog("[%p: %s]\n", sandbox, sandbox->module->name);
	assert(sandbox->state == SANDBOX_BLOCKED);
	sandbox_set_as_runnable(sandbox, NULL);
	software_interrupt_enable();
}


/**
 * Mark the currently executing sandbox as blocked, remove it from the local runqueue, and pull the sandbox at
 * the head of the runqueue
 * TODO: What happens if we block on a sandbox that has preempted something? Should we try to restore first?
 * Is this accomplished by the runqueue design?
 **/
void
worker_thread_block_current_sandbox()
{
	assert(worker_thread_is_in_libuv_event_loop == false);
	software_interrupt_disable();

	// Remove the sandbox we were just executing from the runqueue and mark as blocked
	struct sandbox *current_sandbox = current_sandbox_get();
	assert(current_sandbox->state == SANDBOX_RUNNING);
	sandbox_set_as_blocked(current_sandbox);
	current_sandbox_set(NULL);

	// Try to get another sandbox to run
	struct sandbox *next_sandbox = sandbox_run_queue_get_next();

	// If able to get one, switch to it. Otherwise, return to base context
	if (next_sandbox == NULL) {
		worker_thread_switch_to_base_context();
	} else {
		debuglog("[%p: %next_sandbox, %p: %next_sandbox]\n", current_sandbox, current_sandbox->module->name,
		         next_sandbox, next_sandbox ? next_sandbox->module->name : "");
		software_interrupt_enable();
		worker_thread_switch_to_sandbox(next_sandbox);
	}
}


/**
 * Execute I/O
 **/
void
worker_thread_process_io(void)
{
#ifdef USE_HTTP_UVIO
#ifdef USE_HTTP_SYNC
	// realistically, we're processing all async I/O on this core when a sandbox blocks on http processing,
	// not great! if there is a way (TODO), perhaps RUN_ONCE and check if your I/O is processed, if yes,
	// return else do async block!
	uv_run(worker_thread_get_libuv_handle(), UV_RUN_DEFAULT);
#else  /* USE_HTTP_SYNC */
	worker_thread_block_current_sandbox();
#endif /* USE_HTTP_UVIO */
#else
	assert(false);
	// it should not be called if not using uvio for http
#endif
}

/**
 * Sends the current thread a SIGUSR1, causing a preempted sandbox to be restored
 * Invoked by asm during a context switch
 **/
void __attribute__((noinline)) __attribute__((noreturn)) worker_thread_mcontext_restore(void)
{
	printf("Thread %lu | Signaling SIGUSR1 on self to initiate mcontext restore...\n", pthread_self());
	pthread_kill(pthread_self(), SIGUSR1);
	assert(false); // should not get here..
}

/**
 * Run all outstanding events in the local thread's libuv event loop
 **/
void
worker_thread_execute_libuv_event_loop(void)
{
	worker_thread_is_in_libuv_event_loop = true;
	int n = uv_run(worker_thread_get_libuv_handle(), UV_RUN_NOWAIT), i = 0;
	while (n > 0) {
		n--;
		uv_run(worker_thread_get_libuv_handle(), UV_RUN_NOWAIT);
	}
	worker_thread_is_in_libuv_event_loop = false;
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

	// sandbox_run_queue_fifo_initialize();
	sandbox_run_queue_ps_initialize();

	sandbox_completion_queue_initialize();

	software_interrupt_is_disabled = false;
	worker_thread_next_context     = NULL;

#ifndef PREEMPT_DISABLE
	software_interrupt_unmask_signal(SIGALRM);
	software_interrupt_unmask_signal(SIGUSR1);
#endif

	uv_loop_init(&worker_thread_uvio_handle);
	worker_thread_is_in_libuv_event_loop = false;

	// Begin Worker Execution Loop
	struct sandbox *current_sandbox, *next_sandbox;
	while (true) {
		// This only is here to neurotically check that current_sandbox is NULL
		current_sandbox = current_sandbox_get();
		if (current_sandbox != NULL) {
			printf("Worker loop expected current_sandbox to be NULL, but found sandbox in %s state\n",
			       sandbox_state_stringify(current_sandbox->state));
			assert(0);
		}

		// Execute libuv event loop
		if (!worker_thread_is_in_libuv_event_loop) worker_thread_execute_libuv_event_loop();

		// Try to get a new sandbox to execute
		software_interrupt_disable();
		next_sandbox = sandbox_run_queue_get_next();
		software_interrupt_enable();

		// If successful, run it
		if (next_sandbox != NULL) worker_thread_switch_to_sandbox(next_sandbox);

		// Free all sandboxes on the completion queue
		sandbox_completion_queue_free();
	}

	*(int *)return_code = -1;
	pthread_exit(return_code);
}

/**
 * Called when the function in the sandbox exits
 * Removes the standbox from the thread-local runqueue, sets its state to RETURNED,
 * releases the linear memory, and then returns to the base context
 **/
void
worker_thread_on_sandbox_exit(sandbox_t *exiting_sandbox)
{
	assert(exiting_sandbox);
	worker_thread_switch_to_base_context();
}
