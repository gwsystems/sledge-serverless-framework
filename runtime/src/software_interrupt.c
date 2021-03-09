#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>

#include "arch/context.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "local_runqueue.h"
#include "module.h"
#include "panic.h"
#include "runtime.h"
#include "sandbox.h"
#include "software_interrupt.h"

/*******************
 * Process Globals *
 ******************/

static const int software_interrupt_supported_signals[] = { SIGALRM, SIGUSR1 };
uint64_t         software_interrupt_interval_duration_in_cycles;

/******************
 * Thread Globals *
 *****************/

__thread static volatile sig_atomic_t software_interrupt_SIGALRM_count = 0;
__thread static volatile sig_atomic_t software_interrupt_SIGUSR_count  = 0;
__thread volatile sig_atomic_t        software_interrupt_is_disabled   = 0;

/***************************************
 * Externs
 **************************************/

extern pthread_t runtime_worker_threads[];

/**************************
 * Private Static Inlines *
 *************************/

static inline void software_interrupt_handle_signals(int signal_type, siginfo_t *signal_info, void *user_context_raw);


/**
 * A POSIX signal is delivered to only one thread.
 * This function broadcasts the sigalarm signal to all other worker threads
 */
static inline void
sigalrm_propagate_workers(siginfo_t *signal_info)
{
	/* Signal was sent directly by the kernel, so forward to other threads */
	if (signal_info->si_code == SI_KERNEL) {
		for (int i = 0; i < runtime_worker_threads_count; i++) {
			if (pthread_self() == runtime_worker_threads[i]) continue;

			/* All threads should have been initialized */
			assert(runtime_worker_threads[i] != 0);

			pthread_kill(runtime_worker_threads[i], SIGALRM);
		}
	} else {
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
	sigalrm_propagate_workers(signal_info);

	software_interrupt_SIGALRM_count++;

	/* NOOP if software interrupts not enabled */
	if (!software_interrupt_is_enabled()) return;

	/*
	 * if a SIGALRM fires while the worker thread is between sandboxes doing runtime tasks such as processing
	 * the epoll loop, performing completion queue cleanup, etc. current_sandbox might be NULL. In this case,
	 * we should just allow return to allow the worker thread to run the main loop until it loads a new sandbox.
	 *
	 * TODO: Consider if this should be an invarient and the worker thread should disable software
	 * interrupts when doing this work. Issue #95
	 */
	if (!current_sandbox) return;

	/*
	 * if a SIGALRM fires while the worker thread executing cleanup of a sandbox, it might be in a RETURNED
	 * state. In this case, we should just allow return to allow the sandbox to complete cleanup, as it is
	 * about to switch to a new sandbox.
	 *
	 * TODO: Consider if this should be an invarient and the worker thread should disable software
	 * interrupts when doing this work. Issue #95 with above
	 */
	if (current_sandbox->state == SANDBOX_RETURNED) return;

	/* Preempt */
	local_runqueue_preempt(user_context);

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

	software_interrupt_SIGUSR_count++;

#ifdef LOG_PREEMPTION
	debuglog("Total SIGUSR1 Received: %d\n", software_interrupt_SIGUSR_count);
	debuglog("Restoring sandbox: %lu, Stack %llu\n", current_sandbox->id,
	         current_sandbox->ctxt.mctx.gregs[REG_RSP]);
#endif

	arch_mcontext_restore(&user_context->uc_mcontext, &current_sandbox->ctxt);

	software_interrupt_enable();

	return;
}

/**
 * Validates that the thread running the signal handler is a known worker thread
 */
static inline void
software_interrupt_validate_worker()
{
#ifndef NDEBUG
	if (!runtime_is_worker()) panic("A non-worker thread received has unexpectedly received a signal!");
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
#ifdef PREEMPT_DISABLE
	panic("Unexpectedly invoked signal handlers when PREEMPT_DISABLE set\n");
#else

	software_interrupt_validate_worker();

	ucontext_t *    user_context    = (ucontext_t *)user_context_raw;
	struct sandbox *current_sandbox = current_sandbox_get();

	switch (signal_type) {
	case SIGALRM: {
		return sigalrm_handler(signal_info, user_context, current_sandbox);
	}
	case SIGUSR1: {
		return sigusr1_handler(signal_info, user_context, current_sandbox);
	}
	default: {
		if (signal_info->si_code == SI_TKILL) {
			panic("Unexpectedly received signal %d from a thread kill, but we have no handler\n",
			      signal_type);
		} else if (signal_info->si_code == SI_KERNEL) {
			panic("Unexpectedly received signal %d from the kernel, but we have no handler\n", signal_type);
		}
	}
	}
#endif
}

/********************
 * Public Functions *
 *******************/

/**
 * Arms the Interval Timer to start in 10ms and then trigger a SIGALRM every 5ms
 */
void
software_interrupt_arm_timer(void)
{
#ifndef PREEMPT_DISABLE
	struct itimerval interval_timer;

	memset(&interval_timer, 0, sizeof(struct itimerval));
	interval_timer.it_value.tv_usec    = SOFTWARE_INTERRUPT_TIME_TO_START_IN_USEC;
	interval_timer.it_interval.tv_usec = SOFTWARE_INTERRUPT_INTERVAL_DURATION_IN_USEC;

	int return_code = setitimer(ITIMER_REAL, &interval_timer, NULL);
	if (return_code) {
		perror("setitimer");
		exit(1);
	}
#endif
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

	for (int i = 0;
	     i < (sizeof(software_interrupt_supported_signals) / sizeof(software_interrupt_supported_signals[0]));
	     i++) {
		int return_code = sigaction(software_interrupt_supported_signals[i], &signal_action, NULL);
		if (return_code) {
			perror("sigaction");
			exit(1);
		}
	}
}
