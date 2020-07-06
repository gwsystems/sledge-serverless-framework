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
__thread arch_context_t *worker_thread_next_context = NULL;

/* context of the runtime thread before running sandboxes or to resume its "main". */
__thread arch_context_t worker_thread_base_context;

/* libuv i/o loop handle per sandboxing thread! */
__thread uv_loop_t worker_thread_uvio_handle;

/* Flag to signify if the thread is currently running callbacks in the libuv event loop */
static __thread bool worker_thread_is_in_callback;

/***********************
 * Worker Thread Logic *
 **********************/

/**
 * @brief Switches to the next sandbox, placing the current sandbox on the completion queue if in RETURNED state
 * @param next_sandbox The Sandbox Context to switch to or NULL, which forces return to base context
 * @return void
 */
static inline void
worker_thread_switch_to_sandbox(struct sandbox *next_sandbox)
{
	arch_context_t *next_register_context = NULL;
	if (next_sandbox != NULL) next_register_context = &next_sandbox->ctxt;

	software_interrupt_disable();

	/* Get the old sandbox we're switching from */
	struct sandbox *previous_sandbox          = current_sandbox_get();
	arch_context_t *previous_register_context = NULL;
	if (previous_sandbox != NULL) previous_register_context = &previous_sandbox->ctxt;

	/* Set the current sandbox to the next */
	current_sandbox_set(next_sandbox);

	/* ...and switch to the associated context.
	Save the context pointer to worker_thread_next_context in case of preemption */
	worker_thread_next_context = next_register_context;
	arch_context_switch(previous_register_context, next_register_context);

	assert(previous_sandbox == NULL || previous_sandbox->state == RUNNABLE || previous_sandbox->state == BLOCKED
	       || previous_sandbox->state == RETURNED);

	/* If the current sandbox we're switching from is in a RETURNED state, add to completion queue */
	if (previous_sandbox != NULL && previous_sandbox->state == RETURNED) {
		local_completion_queue_add(previous_sandbox);
	} else if (previous_sandbox != NULL) {
		debuglog("Switched away from sandbox is state %d\n", previous_sandbox->state);
	}

	software_interrupt_enable();
}

/**
 * Mark a blocked sandbox as runnable and add it to the runqueue
 * @param sandbox the sandbox to check and update if blocked
 */
void
worker_thread_wakeup_sandbox(sandbox_t *sandbox)
{
	software_interrupt_disable();
	// debuglog("[%p: %s]\n", sandbox, sandbox->module->name);
	if (sandbox->state == BLOCKED) {
		sandbox->state = RUNNABLE;
		debuglog("Marking blocked sandbox as runnable\n");
		local_runqueue_add(sandbox);
	}
	software_interrupt_enable();
}


/**
 * Mark the currently executing sandbox as blocked, remove it from the local runqueue, and pull the sandbox at the head
 * of the runqueue
 */
void
worker_thread_block_current_sandbox(void)
{
	assert(worker_thread_is_in_callback == false);
	software_interrupt_disable();

	/* Remove the sandbox we were just executing from the runqueue and mark as blocked */
	struct sandbox *previous_sandbox = current_sandbox_get();
	local_runqueue_delete(previous_sandbox);
	previous_sandbox->state = BLOCKED;

	/* Switch to the next sandbox */
	struct sandbox *next_sandbox = local_runqueue_get_next();
	debuglog("[%p: %next_sandbox, %p: %next_sandbox]\n", previous_sandbox, previous_sandbox->module->name,
	         next_sandbox, next_sandbox ? next_sandbox->module->name : "");
	software_interrupt_enable();
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
 * We need to switch back to a previously preempted thread. The only way to restore all of its registers is to use
 * sigreturn. To get to sigreturn, we need to send ourselves a signal, then update the registers we should return to,
 * then sigreturn (by returning from the handler).
 */
void __attribute__((noinline)) __attribute__((noreturn)) worker_thread_sandbox_switch_preempt(void)
{
	pthread_kill(pthread_self(), SIGUSR1);

	assert(false); /* should not get here.. */
	while (true)
		;
}

/**
 * Run all outstanding events in the local thread's libuv event loop
 */
void
worker_thread_execute_libuv_event_loop(void)
{
	worker_thread_is_in_callback = true;
	int n = uv_run(worker_thread_get_libuv_handle(), UV_RUN_NOWAIT), i = 0;
	while (n > 0) {
		n--;
		uv_run(worker_thread_get_libuv_handle(), UV_RUN_NOWAIT);
	}
	worker_thread_is_in_callback = false;
}

/**
 * The entry function for sandbox worker threads
 * Initializes thread-local state, unmasks signals, sets up libuv loop and
 * @param return_code - argument provided by pthread API. We set to -1 on error
 */
void *
worker_thread_main(void *return_code)
{
	/* Initialize Worker Infrastructure */
	arch_context_init(&worker_thread_base_context, 0, 0);
	// local_runqueue_list_initialize();
	local_runqueue_minheap_initialize();
	local_completion_queue_initialize();
	software_interrupt_is_disabled = false;
	worker_thread_next_context     = NULL;
#ifndef PREEMPT_DISABLE
	software_interrupt_unmask_signal(SIGALRM);
	software_interrupt_unmask_signal(SIGUSR1);
#endif
	uv_loop_init(&worker_thread_uvio_handle);
	worker_thread_is_in_callback = false;

	/* Begin Worker Execution Loop */
	struct sandbox *next_sandbox;
	while (true) {
		assert(current_sandbox_get() == NULL);
		/* If "in a callback", the libuv event loop is triggering this, so we don't need to start it */
		if (!worker_thread_is_in_callback) worker_thread_execute_libuv_event_loop();

		software_interrupt_disable();
		next_sandbox = local_runqueue_get_next();
		software_interrupt_enable();

		if (next_sandbox != NULL) worker_thread_switch_to_sandbox(next_sandbox);

		local_completion_queue_free();
	}

	*(int *)return_code = -1;
	pthread_exit(return_code);
}

/**
 * Called when the function in the sandbox exits
 * Removes the standbox from the thread-local runqueue, sets its state to RETURNED,
 * releases the linear memory, and then switches to the sandbox at the head of the runqueue
 * TODO: Consider moving this to a future current_sandbox file. This has thus far proven difficult to move
 */
void
worker_thread_on_sandbox_exit(sandbox_t *exiting_sandbox)
{
	assert(exiting_sandbox);

	/* TODO: I do not understand when software interrupts must be disabled? */
	software_interrupt_disable();
	local_runqueue_delete(exiting_sandbox);
	exiting_sandbox->state = RETURNED;
	software_interrupt_enable();

	/* Because the stack is still in use, only unmap linear memory and defer free resources until "main
	function execution" */
	errno  = 0;
	int rc = munmap(exiting_sandbox->linear_memory_start, SBOX_MAX_MEM + PAGE_SIZE);
	if (rc == -1) panic("worker_thread_on_sandbox_exit - munmap failed with errno - %s\n", strerror(errno));

	local_completion_queue_add(exiting_sandbox);

	/* This should force return to main event loop */
	worker_thread_switch_to_sandbox(NULL);
}
