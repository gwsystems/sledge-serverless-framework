#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>

#include <types.h>
#include <sandbox.h>
#include <module.h>
#include <arch/context.h>
#include <software_interrupt.h>
#include <current_sandbox.h>
#include "sandbox_run_queue.h"

/***************************************
 * Process Globals
 ***************************************/

static const int software_interrupt_supported_signals[] = { SIGALRM, SIGUSR1 };
uint64_t         SOFTWARE_INTERRUPT_INTERVAL_DURATION_IN_CYCLES;

/***************************************
 * Thread Globals
 ***************************************/

__thread static volatile sig_atomic_t software_interrupt_SIGALRM_count = 0;
__thread static volatile sig_atomic_t software_interrupt_SIGUSR_count  = 0;
__thread volatile sig_atomic_t        software_interrupt_is_disabled   = 0;

/***************************************
 * Externs
 ***************************************/

extern pthread_t runtime_worker_threads[];

/***************************************
 * Private Static Inlines
 ***************************************/

static inline void software_interrupt_handle_signals(int signal_type, siginfo_t *signal_info, void *user_context_raw);
static inline void software_interrupt_schedule_alarm(void *user_context_raw);

/**
 * The handler function for Software Interrupts (signals)
 * SIGALRM is executed periodically by an interval timer, causing preemption of the current sandbox
 * SIGUSR1 restores a preempted sandbox
 * @param signal_type
 * @param signal_info data structure containing signal info
 * @param user_context_raw void* to a user_context struct
 **/
static inline void
software_interrupt_handle_signals(int signal_type, siginfo_t *signal_info, void *user_context_raw)
{
#ifdef PREEMPT_DISABLE
	assert(0);
#else
	ucontext_t *    user_context    = (ucontext_t *)user_context_raw;
	struct sandbox *current_sandbox = current_sandbox_get();

	switch (signal_type) {
	case SIGALRM: {
		// SIGALRM is the preemption signal that occurs every quantum of execution


		if (signal_info->si_code == SI_KERNEL) {
			// deliver signal to all other worker threads..
			for (int i = 0; i < WORKER_THREAD_CORE_COUNT; i++) {
				if (pthread_self() == runtime_worker_threads[i]) continue;
				pthread_kill(runtime_worker_threads[i], SIGALRM);
			}
		} else {
			// What is this?
			assert(signal_info->si_code == SI_TKILL);
		}
		// debuglog("alrm:%d\n", software_interrupt_SIGALRM_count);

		software_interrupt_SIGALRM_count++;
		// software_interrupt_supported_signals per-core..
		if (current_sandbox && current_sandbox->state == RETURNED) return;
		if (worker_thread_next_context) return;
		if (!software_interrupt_is_enabled()) return;
		software_interrupt_schedule_alarm(user_context);

		break;
	}
	case SIGUSR1: {
		// make sure sigalrm doesn't mess this up if nested..
		assert(!software_interrupt_is_enabled());
		/* we set current before calling pthread_kill! */
		assert(worker_thread_next_context && (&current_sandbox->ctxt == worker_thread_next_context));
		assert(signal_info->si_code == SI_TKILL);
		// debuglog("usr1:%d\n", software_interrupt_SIGUSR_count);

		software_interrupt_SIGUSR_count++;
		// do not save current sandbox.. it is in co-operative switch..
		// pick the next from "worker_thread_next_context"..
		// assert its "sp" to be zero in regs..
		// memcpy from next context..
		arch_mcontext_restore(&user_context->uc_mcontext, &current_sandbox->ctxt);
		worker_thread_next_context = NULL;
		software_interrupt_enable();
		break;
	}
	default:
		break;
	}
#endif
}

/**
 * Preempt the current sandbox and start executing the next sandbox
 * @param user_context_raw void* to a user_context struct
 **/
static inline void
software_interrupt_schedule_alarm(void *user_context_raw)
{
	software_interrupt_disable(); // no nesting!
	struct sandbox *current_sandbox                  = current_sandbox_get();
	bool            should_enable_software_interrupt = true;

	// If current_sandbox is null, there's nothing to preempt, so let the "main" scheduler run its course.
	if (current_sandbox != NULL) {
		struct sandbox *next_sandbox = sandbox_run_queue_get_next();

		if (next_sandbox != NULL && next_sandbox != current_sandbox) {
			ucontext_t *user_context = (ucontext_t *)user_context_raw;

			// Save the context of the currently executing sandbox before switching from it
			arch_mcontext_save(&current_sandbox->ctxt, &user_context->uc_mcontext);

			// Update current_sandbox to the next sandbox
			current_sandbox_set(next_sandbox);

			// And load the context of this new sandbox
			// RC of 1 indicates that sandbox was last in a user-level context switch state,
			// so do not enable software interrupts.
			if (arch_mcontext_restore(&user_context->uc_mcontext, &next_sandbox->ctxt) == 1)
				should_enable_software_interrupt = false;
		}
	}

	if (should_enable_software_interrupt) software_interrupt_enable();
}

/***************************************
 * Public Functions
 ***************************************/

/**
 * Arms the Interval Timer to start in 10ms and then trigger a SIGALRM every 5ms
 **/
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
 **/
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
 * Register sonftint_handler to execute on SIGALRM and SIGUSR1
 **/
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
