#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <sys/mman.h>
#include <uv.h>

#include "current_sandbox.h"
#include "global_request_scheduler.h"
#include "local_completion_queue.h"
#include "local_runqueue.h"
#include "local_runqueue_list.h"
#include "local_runqueue_minheap.h"
#include "panic.h"
#include "runtime.h"
#include "types.h"
#include "worker_thread.h"

/***************************
 * Worker Thread State     *
 **************************/

/* context of the runtime thread before running sandboxes or to resume its "main". */
__thread struct arch_context worker_thread_base_context;

/* libuv i/o loop handle per sandboxing thread! */
__thread uv_loop_t worker_thread_uvio_handle;

/* Flag to signify if the thread is currently running callbacks in the libuv event loop */
__thread static bool worker_thread_is_in_libuv_event_loop = false;

/* Flag to signify if the thread is currently undergoing a context switch */
__thread volatile bool worker_thread_is_switching_context = false;

/***********************
 * Worker Thread Logic *
 **********************/

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
		/*
		 * We draw a distinction between RETURNED and COMPLETED because a sandbox cannot add itself to the
		 * completion queue
		 */
		sandbox_set_as_complete(exiting_sandbox);
		break;
	case SANDBOX_ERROR:
		/* Terminal State, so just break */
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
	/* Assumption: The caller disables interrupts */
	assert(software_interrupt_is_disabled);

	assert(next_sandbox != NULL);
	struct arch_context *next_context = &next_sandbox->ctxt;

	worker_thread_is_switching_context = true;

	struct sandbox *current_sandbox = current_sandbox_get();

	if (current_sandbox == NULL) {
		/* Switching from "Base Context" */
		sandbox_set_as_running(next_sandbox);

		debuglog("Thread %lu | Switching from Base Context to Sandbox %lu\n", pthread_self(),
		         next_sandbox->allocation_timestamp);

		arch_context_switch(NULL, next_context);
	} else {
		/* Switching between sandboxes */
		assert(next_sandbox != current_sandbox);

		worker_thread_transition_exiting_sandbox(current_sandbox);

		sandbox_set_as_running(next_sandbox);

		debuglog("Thread %lu | Switching from Sandbox %lu to Sandbox %lu\n", pthread_self(),
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
	/* FIXME: This feels like it should be set. Why are we asserting false? */
	assert(worker_thread_is_switching_context == false);

	software_interrupt_disable();

	struct sandbox *current_sandbox = current_sandbox_get();

	/* Assumption: Base Context should never switch to Base Context */
	assert(current_sandbox != NULL);
	assert(&current_sandbox->ctxt != &worker_thread_base_context);

	worker_thread_transition_exiting_sandbox(current_sandbox);

	current_sandbox_set(NULL);

	debuglog("Thread %lu | Switching from Sandbox %lu to Base Context\n", pthread_self(),
	         current_sandbox->allocation_timestamp);

	arch_context_switch(&current_sandbox->ctxt, &worker_thread_base_context);

	software_interrupt_enable();
}

/**
 * Mark a blocked sandbox as runnable and add it to the runqueue
 * @param sandbox the sandbox to check and update if blocked
 */
void
worker_thread_wakeup_sandbox(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_BLOCKED);

	software_interrupt_disable();
	sandbox_set_as_runnable(sandbox);
	software_interrupt_enable();
}


/**
 * Mark the currently executing sandbox as blocked, remove it from the local runqueue, and pull the sandbox at
 * the head of the runqueue
 * FIXME: What happens if we block on a sandbox that has preempted something? Should we try to restore first?
 * Is this accomplished by the runqueue design?
 **/
void
worker_thread_block_current_sandbox(void)
{
	assert(worker_thread_is_in_libuv_event_loop == false);
	software_interrupt_disable();

	/* Remove the sandbox we were just executing from the runqueue and mark as blocked */
	struct sandbox *current_sandbox = current_sandbox_get();
	assert(current_sandbox->state == SANDBOX_RUNNING);
	sandbox_set_as_blocked(current_sandbox);
	current_sandbox_set(NULL);

	/* Try to get another sandbox to run */
	struct sandbox *next_sandbox = local_runqueue_get_next();

	/* If able to get one, switch to it. Otherwise, return to base context */
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
 */
void
worker_thread_process_io(void)
{
#ifdef USE_HTTP_UVIO
#ifdef USE_HTTP_SYNC
	/* realistically, we're processing all async I/O on this core when a sandbox blocks on http processing, not
	 * great! if there is a way (TODO), perhaps RUN_ONCE and check if your I/O is processed, if yes, return else do
	 * async block! */
	uv_run(worker_thread_get_libuv_handle(), UV_RUN_DEFAULT);
#else  /* USE_HTTP_SYNC */
	worker_thread_block_current_sandbox();
#endif /* USE_HTTP_UVIO */
#else
	assert(false);
	/* it should not be called if not using uvio for http */
#endif
}

/**
 * Run all outstanding events in the local thread's libuv event loop
 */
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
 */
void *
worker_thread_main(void *return_code)
{
	/* Initialize Base Context */
	arch_context_init(&worker_thread_base_context, 0, 0);

	/* Initialize Runqueue Variant */
	// local_runqueue_list_initialize();
	local_runqueue_minheap_initialize();

	/* Initialize Completion Queue */
	local_completion_queue_initialize();

	/* Initialize Flags */
	software_interrupt_is_disabled       = false;
	worker_thread_is_in_libuv_event_loop = false;
	worker_thread_is_switching_context   = false;

	/* Unmask signals */
#ifndef PREEMPT_DISABLE
	software_interrupt_unmask_signal(SIGALRM);
	software_interrupt_unmask_signal(SIGUSR1);
#endif

	/* Initialize libuv event loop handle */
	uv_loop_init(&worker_thread_uvio_handle);

	/* Begin Worker Execution Loop */
	struct sandbox *current_sandbox, *next_sandbox;
	while (true) {
		/* Assumption: current_sandbox should be unset at start of loop */
		current_sandbox = current_sandbox_get();
		if (current_sandbox != NULL) {
			panic("Worker loop expected current_sandbox to be NULL, but found sandbox in %s state\n",
			      sandbox_state_stringify(current_sandbox->state));
		}

		/* Execute libuv event loop */
		if (!worker_thread_is_in_libuv_event_loop) worker_thread_execute_libuv_event_loop();

		/* Switch to a sandbox if one is ready to run */
		software_interrupt_disable();
		next_sandbox = local_runqueue_get_next();
		if (next_sandbox != NULL) {
			worker_thread_switch_to_sandbox(next_sandbox);
		} else {
			software_interrupt_enable();
		};

		/* Clear the completion queue */
		local_completion_queue_free();
	}

	panic("Worker Thread unexpectedly completed run loop.");
}

/**
 * Called when the function in the sandbox exits
 * Removes the standbox from the thread-local runqueue, sets its state to SANDBOX_RETURNED,
 * releases the linear memory, and then returns to the base context
 * TODO: Consider moving this to a future current_sandbox file. This has thus far proven difficult to move
 */
__attribute__((noreturn)) void
worker_thread_on_sandbox_exit(struct sandbox *exiting_sandbox)
{
	assert(exiting_sandbox);
	worker_thread_switch_to_base_context();
	assert(0);
}
