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

/*  context pointer used to store and restore a preempted sandbox. SIGUSR1 */
__thread struct arch_context *worker_thread_next_context = NULL;

/* context of the runtime thread before running sandboxes or to resume its "main". */
__thread struct arch_context worker_thread_base_context;

/* libuv i/o loop handle per sandboxing thread! */
__thread uv_loop_t worker_thread_uvio_handle;

/* Flag to signify if the thread is currently running callbacks in the libuv event loop */
static __thread bool worker_thread_is_in_libuv_event_loop = false;

/***********************
 * Worker Thread Logic *
 **********************/

/**
 * @brief Switches to the next sandbox, placing the current sandbox on the completion queue if in SANDBOX_RETURNED state
 * @param next_sandbox The Sandbox Context to switch to or NULL, which forces return to base context
 * @return void
 */
static inline void
worker_thread_switch_to_sandbox(struct sandbox *next_sandbox)
{
	/* Assumption: The caller disables interrupts */
	assert(software_interrupt_is_disabled);

	struct arch_context *next_register_context = NULL;
	if (next_sandbox != NULL) next_register_context = &next_sandbox->ctxt;

	/* Get the old sandbox we're switching from */
	struct sandbox *     previous_sandbox          = current_sandbox_get();
	struct arch_context *previous_register_context = NULL;
	if (previous_sandbox != NULL) previous_register_context = &previous_sandbox->ctxt;

	/* Set the current sandbox to the next */
	current_sandbox_set(next_sandbox);

	/* ...and switch to the associated context.
	 * Save the context pointer to worker_thread_next_context in case of preemption
	 */
	worker_thread_next_context = next_register_context;
	arch_context_switch(previous_register_context, next_register_context);

	/* If previous sandbox is not NULL, ensure a valid state */
	assert(previous_sandbox == NULL || previous_sandbox->state != SANDBOX_INITIALIZING);

	/* If the current sandbox we're switching from is in a SANDBOX_RETURNED state, add to completion queue */
	if (previous_sandbox != NULL && previous_sandbox->state == SANDBOX_RETURNED) {
		panic("Unexpectedly returned to a sandbox in a RETURNED state\n");
	} else if (previous_sandbox != NULL) {
		debuglog("Resumed a sandbox in state %d\n", previous_sandbox->state);
	}

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

	sandbox->state = SANDBOX_RUNNABLE;
	debuglog("Marking blocked sandbox as runnable\n");
	local_runqueue_add(sandbox);

done:
	software_interrupt_enable();
}


/**
 * Mark the currently executing sandbox as blocked, remove it from the local runqueue, and pull the sandbox at the head
 * of the runqueue
 */
void
worker_thread_block_current_sandbox(void)
{
	assert(worker_thread_is_in_libuv_event_loop == false);
	software_interrupt_disable();

	/* Remove the sandbox we were just executing from the runqueue and mark as blocked */
	struct sandbox *previous_sandbox = current_sandbox_get();
	local_runqueue_delete(previous_sandbox);
	previous_sandbox->state = SANDBOX_BLOCKED;

	/* Switch to the next sandbox */
	struct sandbox *next_sandbox = local_runqueue_get_next();
	debuglog("[%p: %next_sandbox, %p: %next_sandbox]\n", previous_sandbox, previous_sandbox->module->name,
	         next_sandbox, next_sandbox ? next_sandbox->module->name : "");
	worker_thread_switch_to_sandbox(next_sandbox);
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
	worker_thread_next_context           = NULL;

	/* Unmask signals */
#ifndef PREEMPT_DISABLE
	software_interrupt_unmask_signal(SIGALRM);
	software_interrupt_unmask_signal(SIGUSR1);
#endif

	/* Initialize libuv event loop handle */
	uv_loop_init(&worker_thread_uvio_handle);

	/* Begin Worker Execution Loop */
	struct sandbox *next_sandbox;
	while (true) {
		/* Assumption: current_sandbox should be unset at start of loop */
		assert(current_sandbox_get() == NULL);

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

	/* Because the stack is still in use, only unmap linear memory and defer free resources until "main
	function execution" */
	errno  = 0;
	int rc = munmap(exiting_sandbox->linear_memory_start, SBOX_MAX_MEM + PAGE_SIZE);
	if (rc == -1) panic("worker_thread_on_sandbox_exit - munmap failed with errno - %s\n", strerror(errno));

	/* TODO: I do not understand when software interrupts must be disabled? */
	software_interrupt_disable();
	local_runqueue_delete(exiting_sandbox);
	exiting_sandbox->state = SANDBOX_RETURNED;
	local_completion_queue_add(exiting_sandbox);

	/* This should force return to main event loop */
	worker_thread_switch_to_sandbox(NULL);

	assert(0);
}
