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

__thread static volatile sig_atomic_t alarm_cnt = 0, usr1_cnt = 0;

__thread volatile sig_atomic_t softint_off = 0;

static const int softints[] = { 
	SIGALRM, 
	SIGUSR1 
	};

void
softint_timer_arm(void)
{
#ifndef PREEMPT_DISABLE
	struct itimerval it;

	memset(&it, 0, sizeof(struct itimerval));
	it.it_value.tv_usec = SOFTINT_TIMER_START_USEC;
	it.it_interval.tv_usec = SOFTINT_TIMER_PERIOD_USEC;

	int ret = setitimer(ITIMER_REAL, &it, NULL);
	if (ret) {
		perror("setitimer");
		exit(1);
	}
#endif
}

void
softint_timer_disarm(void)
{
	struct itimerval it;

	memset(&it, 0, sizeof(struct itimerval));
	it.it_value.tv_sec = 0;
	it.it_interval.tv_usec = 0;

	int ret = setitimer(ITIMER_REAL, &it, NULL);
	if (ret) {
		perror("setitimer");
		exit(1);
	}
}

static inline void
softint_alarm_schedule(void *u)
{
	softint_disable(); //no nesting!

	struct sandbox *curr = sandbox_current();
	ucontext_t *uc = (ucontext_t *)u;
	// no sandboxes running..so nothing to preempt..let the "main" scheduler run its course.
	if (curr == NULL) goto done;
	
	// find a next sandbox to run..
	struct sandbox *next = sandbox_schedule();
	if (next == NULL) goto done;
	if (next == curr) goto done; // only this sandbox to schedule.. return to it!
	// save the current sandbox, state from uc!
	arch_mcontext_save(&curr->ctxt, &uc->uc_mcontext);

	// sandbox_current_set on it. restore through *uc..
	sandbox_current_set(next);

	if (arch_mcontext_restore(&uc->uc_mcontext, &next->ctxt)) goto skip;
	// reset if SIGALRM happens before SIGUSR1 and if don't preempt..OR 
	// perhaps switch here for SIGUSR1 and see if we can clear that signal 
	// so it doesn't get called on SIGALRM return..
	// next_context = NULL;

done:
	softint_enable();
skip:
	return;
}

extern pthread_t rtthd[];

static inline void
softint_handler(int sig, siginfo_t *si, void *u)
{
#ifdef PREEMPT_DISABLE
	assert(0);
#else
	struct sandbox *curr = sandbox_current();
	ucontext_t *uc = (ucontext_t *)u;

	switch(sig) {
	case SIGALRM:
	{
		// if interrupts are disabled.. increment a per_thread counter and return
		if (si->si_code == SI_KERNEL) {
			int rt = 0;
			// deliver signal to all other runtime threads.. 
			for (int i = 0; i < SBOX_NCORES; i++) {
				if (pthread_self() == rtthd[i]) {
					rt = 1;
					continue;
				}
				pthread_kill(rtthd[i], SIGALRM);
			}
			assert(rt == 1);
		} else {
			assert(si->si_code == SI_TKILL);
		}
		//debuglog("alrm:%d\n", alarm_cnt);

		alarm_cnt++;
		// softints per-core..
		if (curr && curr->state == SANDBOX_RETURNED) return;
		if (next_context) return;
		if (!softint_enabled()) return; 
		softint_alarm_schedule(u);

		break;
	}
	case SIGUSR1:
	{
		// make sure sigalrm doesn't mess this up if nested..
		assert(!softint_enabled());
		/* we set current before calling pthread_kill! */
		assert(next_context && (&curr->ctxt == next_context));
		assert(si->si_code == SI_TKILL);
		//debuglog("usr1:%d\n", usr1_cnt);

		usr1_cnt++;
		// do not save current sandbox.. it is in co-operative switch..
		// pick the next from "next_context"..
		// assert its "sp" to be zero in regs..
		// memcpy from next context..
		arch_mcontext_restore(&uc->uc_mcontext, &curr->ctxt);
		next_context = NULL;
		softint_enable();
		break;
	}
	case SIGPIPE:
	case SIGILL:
	case SIGFPE:
	case SIGSEGV:
	{
		// determine if the crash was in the sandbox.. 
		// if (pthread_self() == one_of_the_runtime_threads), a sandbox crashed.. kill it.
		// another check there could be if it is in linear memory or outside, if outside it could repeat with other sandboxes.. so perhaps restart that thread or start a fresh thread??.
		// else, shoot yourself in the head!..
		break;
	}
	default: break;
	}
#endif
}

void
softint_init(void)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = softint_handler;
	sa.sa_flags = SA_SIGINFO | SA_RESTART;

	for (int i = 0; i < (sizeof(softints) / sizeof(softints[0])); i++) {
		int ret = sigaction(softints[i], &sa, NULL);
		if (ret) {
			perror("sigaction");
			exit(1);
		}
	}
}
