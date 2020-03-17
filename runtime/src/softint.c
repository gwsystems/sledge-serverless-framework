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
#include <softint.h>
#include <current_sandbox.h>

/***************************************
 * Process Globals
 ***************************************/

static const int softint__supported_signals[] = { SIGALRM, SIGUSR1 };

/***************************************
 * Thread Globals
 ***************************************/

__thread static volatile sig_atomic_t softint__SIGALRM_count = 0;
__thread static volatile sig_atomic_t softint__SIGUSR_count  = 0;
__thread volatile sig_atomic_t softint__is_disabled = 0;

/***************************************
 * Externs
 ***************************************/

extern pthread_t worker_threads[];

/***************************************
 * Private Static Inlines
 ***************************************/

static inline void softint__handle_signals(int signal_type, siginfo_t *signal_info, void *user_context_raw);
static inline void softint__schedule_alarm(void *user_context_raw);

/**
 * The handler function for Software Interrupts (signals)
 * SIGALRM is executed periodically by an interval timer, causing preemption of the current sandbox
 * SIGUSR1 restores a preempted sandbox
 * @param signal_type
 * @param signal_info data structure containing signal info
 * @param user_context_raw void* to a user_context struct
 **/
static inline void
softint__handle_signals(int signal_type, siginfo_t *signal_info, void *user_context_raw)
{
#ifdef PREEMPT_DISABLE
	assert(0);
#else
	struct sandbox *curr         = current_sandbox__get();
	ucontext_t *    user_context = (ucontext_t *)user_context_raw;

	switch (signal_type) {
	case SIGALRM: {
		// if interrupts are disabled.. increment a per_thread counter and return
		if (signal_info->si_code == SI_KERNEL) {
			int rt = 0;
			// deliver signal to all other runtime threads..
			for (int i = 0; i < SBOX_NCORES; i++) {
				if (pthread_self() == worker_threads[i]) {
					rt = 1;
					continue;
				}
				pthread_kill(worker_threads[i], SIGALRM);
			}
			assert(rt == 1);
		} else {
			assert(signal_info->si_code == SI_TKILL);
		}
		// debuglog("alrm:%d\n", softint__SIGALRM_count);

		softint__SIGALRM_count++;
		// softint__supported_signals per-core..
		if (curr && curr->state == RETURNED) return;
		if (worker_thread__next_context) return;
		if (!softint__is_enabled()) return;
		softint__schedule_alarm(user_context_raw);

		break;
	}
	case SIGUSR1: {
		// make sure sigalrm doesn't mess this up if nested..
		assert(!softint__is_enabled());
		/* we set current before calling pthread_kill! */
		assert(worker_thread__next_context && (&curr->ctxt == worker_thread__next_context));
		assert(signal_info->si_code == SI_TKILL);
		// debuglog("usr1:%d\n", softint__SIGUSR_count);

		softint__SIGUSR_count++;
		// do not save current sandbox.. it is in co-operative switch..
		// pick the next from "worker_thread__next_context"..
		// assert its "sp" to be zero in regs..
		// memcpy from next context..
		arch_mcontext_restore(&user_context->uc_mcontext, &curr->ctxt);
		worker_thread__next_context = NULL;
		softint__enable();
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
softint__schedule_alarm(void *user_context_raw)
{
	softint__disable(); // no nesting!

	struct sandbox *curr         = current_sandbox__get();
	ucontext_t *    user_context = (ucontext_t *)user_context_raw;

	// no sandboxes running..so nothing to preempt..let the "main" scheduler run its course.
	if (curr == NULL) goto done;

	// find a next sandbox to run..
	struct sandbox *next = get_next_sandbox_from_local_run_queue(1);
	if (next == NULL) goto done;
	if (next == curr) goto done; // only this sandbox to schedule.. return to it!
	// save the current sandbox, state from user_context!
	arch_mcontext_save(&curr->ctxt, &user_context->uc_mcontext);

	// current_sandbox__set on it. restore through *user_context..
	current_sandbox__set(next);

	if (arch_mcontext_restore(&user_context->uc_mcontext, &next->ctxt)) goto skip;
	// reset if SIGALRM happens before SIGUSR1 and if don't preempt..OR
	// perhaps switch here for SIGUSR1 and see if we can clear that signal
	// so it doesn't get called on SIGALRM return..
	// worker_thread__next_context = NULL;

done:
	softint__enable();
skip:
	return;
}

/***************************************
 * Public Functions
 ***************************************/

/**
 * Arms the Interval Timer to start in 10ms and then trigger a SIGALRM every 5ms
 **/
void
softint__arm_timer(void)
{
#ifndef PREEMPT_DISABLE
	struct itimerval interval_timer;

	memset(&interval_timer, 0, sizeof(struct itimerval));
	interval_timer.it_value.tv_usec    = SOFTINT_TIMER_START_USEC;
	interval_timer.it_interval.tv_usec = SOFTINT_TIMER_PERIOD_USEC;

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
softint__disarm_timer(void)
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
softint__initialize(void)
{
	struct sigaction signal_action;
	memset(&signal_action, 0, sizeof(struct sigaction));
	signal_action.sa_sigaction = softint__handle_signals;
	signal_action.sa_flags     = SA_SIGINFO | SA_RESTART;

	for (int i = 0; i < (sizeof(softint__supported_signals) / sizeof(softint__supported_signals[0])); i++) {
		int return_code = sigaction(softint__supported_signals[i], &signal_action, NULL);
		if (return_code) {
			perror("sigaction");
			exit(1);
		}
	}
}
