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

		// A POSIX signal is delivered to one of the threads in our process. If sent by the kernel, "broadcast"
		// by forwarding to all all threads
		if (signal_info->si_code == SI_KERNEL) {
			for (int i = 0; i < WORKER_THREAD_CORE_COUNT; i++) {
				if (pthread_self() == runtime_worker_threads[i]) continue;
				pthread_kill(runtime_worker_threads[i], SIGALRM);
			}
		} else {
			// If not sent by the kernel, this should be a signal forwarded from another thread
			assert(signal_info->si_code == SI_TKILL);
		}
		// debuglog("alrm:%d\n", software_interrupt_SIGALRM_count);

		software_interrupt_SIGALRM_count++;

		// if the current sandbox is NULL or in a RETURNED state, nothing to preempt, so just return
		if (!current_sandbox) return;
		if (current_sandbox && current_sandbox->state == SANDBOX_RETURNED) return;

		// If worker_thread_next_context is not NULL, we have already preempted a sandbox
		// This approach means that we can only have up to two sandboxes in our worker runqueue
		// TODO: Use the PQ runqueue to preempt to arbitrary depth
		if (worker_thread_next_context) return;

		// If software interrupts are disabled, return
		if (!software_interrupt_is_enabled()) return;

		// Preempt
		sandbox_run_queue_preempt(user_context);

		return;
	}
	case SIGUSR1: {
		// SIGUSR1 restores the preempted sandbox stored in worker_thread_next_context

		// TODO: This was somehow causing sandboxes already acti
		// If worker_thread_next_context is NULL, assume the preempted sandbox was already resumed
		if (worker_thread_next_context == NULL) {
			if (!software_interrupt_is_enabled()) software_interrupt_enable();
			break;
		};

		// Invariants

		// Software Interrupts are disabled in userspace (this has nothing to do with masking / unmasking)
		assert(!software_interrupt_is_enabled());

		// The worker thread was running a sandbox when preempted
		assert(current_sandbox != NULL);

		// The current sandbox's context is pointed to by worker_thread_next_context
		// TODO: Revisit this in the future
		assert(worker_thread_next_context && (&current_sandbox->ctxt == worker_thread_next_context));

		// The signal was sent by a thread, not the kernel
		assert(signal_info->si_code == SI_TKILL);

		// The current sandbox has a valid Instruction Pointer
		// TODO: What is the state here???
		// assert(current_sandbox->ctxt.regs[UREG_RSP] != 0);
		// assert(current_sandbox->ctxt.regs[UREG_RIP] != 0);

		// debuglog("usr1:%d\n", software_interrupt_SIGUSR_count);
		software_interrupt_SIGUSR_count++;

		// TODO: For some reason, a sandbox can be in the SANDBOX_RUNNING state here...
		// This feels like a hack, but just exit in this case.
		if (current_sandbox->state == SANDBOX_PREEMPTED)
			sandbox_set_as_running(current_sandbox, &user_context->uc_mcontext);
		worker_thread_next_context = NULL;
		software_interrupt_enable();
		break;
	}
	default:
		break;
	}
#endif
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
