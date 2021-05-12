#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>

#include "arch/context.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "global_request_scheduler.h"
#include "listener_thread.h"
#include "local_runqueue.h"
#include "module.h"
#include "panic.h"
#include "runtime.h"
#include "sandbox_types.h"
#include "software_interrupt.h"

/*******************
 * Process Globals *
 ******************/

static const int software_interrupt_supported_signals[] = { SIGALRM, SIGUSR1 };
static uint64_t  software_interrupt_interval_duration_in_cycles;

/******************
 * Thread Globals *
 *****************/

__thread _Atomic static volatile sig_atomic_t software_interrupt_SIGALRM_kernel_count = 0;
__thread _Atomic static volatile sig_atomic_t software_interrupt_SIGALRM_thread_count = 0;
__thread _Atomic static volatile sig_atomic_t software_interrupt_SIGUSR_count         = 0;
__thread volatile sig_atomic_t                software_interrupt_is_disabled          = 1;
__thread _Atomic volatile sig_atomic_t        software_interrupt_deferred_sigalrm     = 0;
__thread _Atomic volatile sig_atomic_t        software_interrupt_signal_depth         = 0;

_Atomic volatile sig_atomic_t software_interrupt_deferred_sigalrm_max[RUNTIME_WORKER_THREAD_CORE_COUNT] = { 0 };

void
software_interrupt_deferred_sigalrm_max_print()
{
	printf("Max Deferred Sigalrms\n");
	for (int i = 0; i < runtime_worker_threads_count; i++) {
		printf("Worker %d: %d\n", i, software_interrupt_deferred_sigalrm_max[i]);
	}
	fflush(stdout);
}

/***************************************
 * Externs
 **************************************/

extern pthread_t runtime_worker_threads[];

/**************************
 * Private Static Inlines *
 *************************/

/**
 * A POSIX signal is delivered to only one thread.
 * This function broadcasts the sigalarm signal to all other worker threads
 */
static inline void
sigalrm_propagate_workers(siginfo_t *signal_info)
{
	/* Signal was sent directly by the kernel, so forward to other threads */
	if (signal_info->si_code == SI_KERNEL) {
		atomic_fetch_add(&software_interrupt_SIGALRM_kernel_count, 1);
		for (int i = 0; i < runtime_worker_threads_count; i++) {
			if (pthread_self() == runtime_worker_threads[i]) continue;

			/* All threads should have been initialized */
			assert(runtime_worker_threads[i] != 0);

			/* If using EDF, conditionally send signals. If not, broadcast */
			switch (runtime_sigalrm_handler) {
			case RUNTIME_SIGALRM_HANDLER_TRIAGED: {
				uint64_t local_deadline  = runtime_worker_threads_deadline[i];
				uint64_t global_deadline = global_request_scheduler_peek();
				if (global_deadline < local_deadline) pthread_kill(runtime_worker_threads[i], SIGALRM);
				continue;
			}
			case RUNTIME_SIGALRM_HANDLER_BROADCAST: {
				pthread_kill(runtime_worker_threads[i], SIGALRM);
				continue;
			}
			default: {
				panic("Unexpected SIGALRM Handler: %d\n", runtime_sigalrm_handler);
			}
			}
		}
	} else {
		atomic_fetch_add(&software_interrupt_SIGALRM_thread_count, 1);
		/* Signal forwarded from another thread. Just confirm it resulted from pthread_kill */
		assert(signal_info->si_code == SI_TKILL);
	}
}

/**
 * SIGALRM is the preemption signal that occurs every quantum of execution
 * @param signal_info data structure containing signal info
 * @param user_context userland context
 * @param current_sandbox the sanbox active on the worker thread
 */
static inline void
sigalrm_handler(siginfo_t *signal_info, ucontext_t *user_context, struct sandbox *current_sandbox)
{
	/* A worker thread received a SIGALRM when interrupts were disabled, so defer until they are reenabled */
	if (!software_interrupt_is_enabled()) {
		// Don't increment if kernel? The first worker gets tons of these...
		atomic_fetch_add(&software_interrupt_deferred_sigalrm, 1);
		return;
	}

	/* A worker thread received a SIGALRM while running a preemptable sandbox, so preempt */
	assert(current_sandbox->ctxt.preemptable);
	software_interrupt_disable();

	assert(current_sandbox != NULL);
	assert(current_sandbox->state != SANDBOX_RETURNED);

	/* Preempt */
	local_runqueue_preempt(user_context);

	/* We have to call current_sandbox_get because the argument potentially points to what
	 * was just preempted */
	if (current_sandbox_get()->ctxt.preemptable) software_interrupt_enable();

	return;
}

/**
 * SIGUSR1 restores a preempted sandbox using mcontext
 * @param signal_info data structure containing signal info
 * @param user_context userland context
 * @param current_sandbox the sanbox active on the worker thread
 */
static inline void
sigusr1_handler(siginfo_t *signal_info, ucontext_t *user_context, struct sandbox *current_sandbox)
{
	/* Assumption: Caller disables interrupt before triggering SIGUSR1 */
	assert(!software_interrupt_is_enabled());

	/* Assumption: Caller sets current_sandbox to the preempted sandbox */
	assert(current_sandbox);

	/* Extra checks to verify that preemption properly set context state */
	assert(current_sandbox->ctxt.variant == ARCH_CONTEXT_VARIANT_SLOW);

	atomic_fetch_add(&software_interrupt_SIGUSR_count, 1);

#ifdef LOG_PREEMPTION
	debuglog("Total SIGUSR1 Received: %d\n", software_interrupt_SIGUSR_count);
	debuglog("Restoring sandbox: %lu, Stack %llu\n", current_sandbox->id,
	         current_sandbox->ctxt.mctx.gregs[REG_RSP]);
#endif

	arch_mcontext_restore(&user_context->uc_mcontext, &current_sandbox->ctxt);

	return;
}

/**
 * Validates that the thread running the signal handler is a known worker thread
 */
static inline void
software_interrupt_validate_worker()
{
#ifndef NDEBUG
	if (listener_thread_is_running()) panic("The listener thread unexpectedly received a signal!");
#endif
}

/**
 * The handler function for Software Interrupts (signals)
 * SIGALRM is executed periodically by an interval timer, causing preemption of the current sandbox
 * SIGUSR1 restores a preempted sandbox
 * @param signal_type
 * @param signal_info data structure containing signal info
 * @param user_context_raw void* to a user_context struct
 */
static inline void
software_interrupt_handle_signals(int signal_type, siginfo_t *signal_info, void *user_context_raw)
{
	/* If the runtime has preemption disabled, and we receive a signal, panic */
	if (unlikely(!runtime_preemption_enabled)) {
		panic("Unexpectedly invoked signal handlers with preemption disabled\n");
	}

	assert(software_interrupt_signal_depth < 2);
	atomic_fetch_add(&software_interrupt_signal_depth, 1);
	software_interrupt_validate_worker();

	ucontext_t *    user_context    = (ucontext_t *)user_context_raw;
	struct sandbox *current_sandbox = current_sandbox_get();

	switch (signal_type) {
	case SIGALRM: {
		sigalrm_handler(signal_info, user_context, current_sandbox);
		break;
	}
	case SIGUSR1: {
		sigusr1_handler(signal_info, user_context, current_sandbox);
		break;
	}
	default: {
		switch (signal_info->si_code) {
		case SI_TKILL:
			panic("Unexpectedly received signal %d from a thread kill, but we have no handler\n",
			      signal_type);
		case SI_KERNEL:
			panic("Unexpectedly received signal %d from the kernel, but we have no handler\n", signal_type);
		default:
			panic("Anomolous Signal\n");
		}
	}
	}
	atomic_fetch_sub(&software_interrupt_signal_depth, 1);
}

/********************
 * Public Functions *
 *******************/

/**
 * Arms the Interval Timer to start in one quantum and then trigger a SIGALRM every quantum
 */
void
software_interrupt_arm_timer(void)
{
	if (!runtime_preemption_enabled) return;

	struct itimerval interval_timer;

	memset(&interval_timer, 0, sizeof(struct itimerval));
	interval_timer.it_value.tv_usec    = runtime_quantum_us;
	interval_timer.it_interval.tv_usec = runtime_quantum_us;

	int return_code = setitimer(ITIMER_REAL, &interval_timer, NULL);
	if (return_code) {
		perror("setitimer");
		exit(1);
	}
}

/**
 * Disarm the Interval Timer
 */
void
software_interrupt_disarm_timer(void)
{
	struct itimerval interval_timer;

	memset(&interval_timer, 0, sizeof(struct itimerval));
	interval_timer.it_value.tv_sec     = 0;
	interval_timer.it_interval.tv_usec = 0;

	int return_code = setitimer(ITIMER_REAL, &interval_timer, NULL);
	if (return_code) {
		perror("setitimer");
		exit(1);
	}
}


/**
 * Initialize software Interrupts
 * Register softint_handler to execute on SIGALRM and SIGUSR1
 */
void
software_interrupt_initialize(void)
{
	struct sigaction signal_action;
	memset(&signal_action, 0, sizeof(struct sigaction));
	signal_action.sa_sigaction = software_interrupt_handle_signals;
	signal_action.sa_flags     = SA_SIGINFO | SA_RESTART;

	/* all threads created by the calling thread will have signal blocked */

	// TODO: Unclear about this...
	sigemptyset(&signal_action.sa_mask);
	// sigaddset(&signal_action.sa_mask, SIGALRM);
	// sigaddset(&signal_action.sa_mask, SIGUSR1);

	for (int i = 0;
	     i < (sizeof(software_interrupt_supported_signals) / sizeof(software_interrupt_supported_signals[0]));
	     i++) {
		// TODO: Setup masks
		int return_code = sigaction(software_interrupt_supported_signals[i], &signal_action, NULL);
		if (return_code) {
			perror("sigaction");
			exit(1);
		}
	}
}

void
software_interrupt_set_interval_duration(uint64_t cycles)
{
	software_interrupt_interval_duration_in_cycles = cycles;
}
