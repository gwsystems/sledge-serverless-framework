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

__thread static volatile sig_atomic_t SIGALRM_count = 0;
__thread static volatile sig_atomic_t SIGUSR_count  = 0;

__thread volatile sig_atomic_t softint_off = 0;

static const int softints[] = { SIGALRM, SIGUSR1 };

/**
 * Arms the Interval Timer to start in 10ms and then trigger a SIGALRM every 5ms
 **/
void
softint_timer_arm(void)
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
softint_timer_disarm(void)
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
 * Preempt the current sandbox and start executing the next sandbox
 * @param user_context_raw void* to a user_context struct
 **/
static inline void
softint_alarm_schedule(void *user_context_raw)
{
	softint_disable(); // no nesting!

	struct sandbox *curr         = get_current_sandbox();
	ucontext_t *    user_context = (ucontext_t *)user_context_raw;

	// no sandboxes running..so nothing to preempt..let the "main" scheduler run its course.
	if (curr == NULL) goto done;

	// find a next sandbox to run..
	struct sandbox *next = get_next_sandbox_from_local_run_queue(1);
	if (next == NULL) goto done;
	if (next == curr) goto done; // only this sandbox to schedule.. return to it!
	// save the current sandbox, state from user_context!
	arch_mcontext_save(&curr->ctxt, &user_context->uc_mcontext);

	// set_current_sandbox on it. restore through *user_context..
	set_current_sandbox(next);

	if (arch_mcontext_restore(&user_context->uc_mcontext, &next->ctxt)) goto skip;
	// reset if SIGALRM happens before SIGUSR1 and if don't preempt..OR
	// perhaps switch here for SIGUSR1 and see if we can clear that signal
	// so it doesn't get called on SIGALRM return..
	// next_context = NULL;

done:
	softint_enable();
skip:
	return;
}

extern pthread_t worker_threads[];

/**
 * The handler function for Software Interrupts (signals)
 * SIGALRM is executed periodically by an interval timer, causing preemption of the current sandbox
 * SIGUSR1 does ??????
 * @param signal_type
 * @param signal_info data structure containing signal info
 * @param user_context_raw void* to a user_context struct
 **/
static inline void
softint_handler(int signal_type, siginfo_t *signal_info, void *user_context_raw)
{
#ifdef PREEMPT_DISABLE
	assert(0);
#else
	struct sandbox *curr         = get_current_sandbox();
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
		// debuglog("alrm:%d\n", SIGALRM_count);

		SIGALRM_count++;
		// softints per-core..
		if (curr && curr->state == RETURNED) return;
		if (next_context) return;
		if (!softint_enabled()) return;
		softint_alarm_schedule(user_context_raw);

		break;
	}
	case SIGUSR1: {
		// make sure sigalrm doesn't mess this up if nested..
		assert(!softint_enabled());
		/* we set current before calling pthread_kill! */
		assert(next_context && (&curr->ctxt == next_context));
		assert(signal_info->si_code == SI_TKILL);
		// debuglog("usr1:%d\n", SIGUSR_count);

		SIGUSR_count++;
		// do not save current sandbox.. it is in co-operative switch..
		// pick the next from "next_context"..
		// assert its "sp" to be zero in regs..
		// memcpy from next context..
		arch_mcontext_restore(&user_context->uc_mcontext, &curr->ctxt);
		next_context = NULL;
		softint_enable();
		break;
	}
	default:
		break;
	}
#endif
}

/**
 * Initialize software Interrupts
 * Register sonftint_handler to execute on SIGALRM and SIGUSR1
 **/
void
softint_init(void)
{
	struct sigaction signal_action;
	memset(&signal_action, 0, sizeof(struct sigaction));
	signal_action.sa_sigaction = softint_handler;
	signal_action.sa_flags     = SA_SIGINFO | SA_RESTART;

	for (int i = 0; i < (sizeof(softints) / sizeof(softints[0])); i++) {
		int return_code = sigaction(softints[i], &signal_action, NULL);
		if (return_code) {
			perror("sigaction");
			exit(1);
		}
	}
}
