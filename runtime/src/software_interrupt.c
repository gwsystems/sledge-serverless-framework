#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <threads.h>
#include <unistd.h>
#include <ucontext.h>

#include "arch/context.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "listener_thread.h"
#include "local_runqueue.h"
#include "module.h"
#include "panic.h"
#include "runtime.h"
#include "sandbox_set_as_running_user.h"
#include "sandbox_set_as_interrupted.h"
#include "sandbox_types.h"
#include "scheduler.h"
#include "software_interrupt.h"
#include "software_interrupt_counts.h"

thread_local _Atomic volatile sig_atomic_t handler_depth    = 0;
thread_local _Atomic volatile sig_atomic_t deferred_sigalrm = 0;

/***************************************
 * Externs
 **************************************/

extern pthread_t *runtime_worker_threads;

/**************************
 * Private Static Inlines *
 *************************/

/**
 * A POSIX signal is delivered to only one thread.
 * This function broadcasts the sigalarm signal to all other worker threads
 */
static inline void
propagate_sigalrm(siginfo_t *signal_info)
{
	/* Signal was sent directly by the kernel, so forward to other threads */
	if (signal_info->si_code == SI_KERNEL) {
		software_interrupt_counts_sigalrm_kernel_increment();
		for (int i = 0; i < runtime_worker_threads_count; i++) {
			/* All threads should have been initialized */
			assert(runtime_worker_threads[i] != 0);

			if (pthread_self() == runtime_worker_threads[i]) continue;

			switch (runtime_sigalrm_handler) {
			case RUNTIME_SIGALRM_HANDLER_TRIAGED: {
				if (scheduler_worker_would_preempt(i)) pthread_kill(runtime_worker_threads[i], SIGALRM);
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
		software_interrupt_counts_sigalrm_thread_increment();
		/* Signal forwarded from another thread. Just confirm it resulted from pthread_kill */
		assert(signal_info->si_code == SI_TKILL);
	}
}

static inline bool
worker_thread_is_running_cooperative_scheduler(void)
{
	return current_sandbox_get() == NULL;
}


static inline bool
current_sandbox_is_preemptable()
{
	struct sandbox *sandbox = current_sandbox_get();
	return sandbox != NULL && sandbox->state == SANDBOX_RUNNING_USER;
}

/**
 * The handler function for Software Interrupts (signals)
 * SIGALRM is executed periodically by an interval timer, causing preemption of the current sandbox
 * SIGUSR1 restores a preempted sandbox
 * @param signal_type
 * @param signal_info data structure containing signal info
 * @param interrupted_context_raw void* to a interrupted_context struct
 */
static inline void
software_interrupt_handle_signals(int signal_type, siginfo_t *signal_info, void *interrupted_context_raw)
{
	/* Only workers should receive signals */
	assert(!listener_thread_is_running());

	/* Signals should not nest */
	assert(handler_depth == 0);
	atomic_fetch_add(&handler_depth, 1);

	ucontext_t     *interrupted_context = (ucontext_t *)interrupted_context_raw;
	struct sandbox *current_sandbox     = current_sandbox_get();

	switch (signal_type) {
	case SIGALRM: {
		assert(runtime_preemption_enabled);

		if (worker_thread_is_running_cooperative_scheduler()) {
			/* There is no benefit to deferring SIGALRMs that occur when we are already in the cooperative
			 * scheduler, so just propagate and return */
			if (scheduler == SCHEDULER_MTDS && signal_info->si_code == SI_KERNEL) {
				/* Global tenant promotions */
				global_timeout_queue_process_promotions();
			}
			propagate_sigalrm(signal_info);
		} else if (current_sandbox_is_preemptable()) {
			/* Preemptable, so run scheduler. The scheduler handles outgoing state changes */
			sandbox_interrupt(current_sandbox);
			if (scheduler == SCHEDULER_MTDS && signal_info->si_code == SI_KERNEL) {
				/* Global tenant promotions */
				global_timeout_queue_process_promotions();
			}
			propagate_sigalrm(signal_info);
			scheduler_preemptive_sched(interrupted_context);
		} else {
			/* We transition the sandbox to an interrupted state to exclude time propagating signals and
			 * running the scheduler from per-sandbox accounting */
			sandbox_state_t interrupted_state = current_sandbox->state;
			if (scheduler == SCHEDULER_MTDS && signal_info->si_code == SI_KERNEL) {
				/* Global tenant promotions */
				global_timeout_queue_process_promotions();
			}
			propagate_sigalrm(signal_info);
			atomic_fetch_add(&deferred_sigalrm, 1);
		}

		break;
	}
	case SIGUSR1: {
		assert(runtime_preemption_enabled);
		assert(current_sandbox);
		assert(current_sandbox->state == SANDBOX_PREEMPTED);
		assert(current_sandbox->ctxt.variant == ARCH_CONTEXT_VARIANT_SLOW);

		software_interrupt_counts_sigusr_increment();
#ifdef LOG_PREEMPTION
		debuglog("Restoring sandbox: %lu, Stack %llu\n", current_sandbox->id,
		         current_sandbox->ctxt.mctx.gregs[REG_RSP]);
#endif
		/* It is the responsibility of the caller to invoke current_sandbox_set before triggering the SIGUSR1 */
		scheduler_preemptive_switch_to(interrupted_context, current_sandbox);

		break;
	}
	case SIGFPE: {
		software_interrupt_counts_sigfpe_increment();

		if (likely(current_sandbox && current_sandbox->state == SANDBOX_RUNNING_USER)) {
			atomic_fetch_sub(&handler_depth, 1);
			current_sandbox_trap(WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION);
		} else {
			panic("Runtime SIGFPE\n");
		}

		break;
	}
	case SIGSEGV: {
		software_interrupt_counts_sigsegv_increment();

		if (likely(current_sandbox && current_sandbox->state == SANDBOX_RUNNING_USER)) {
			atomic_fetch_sub(&handler_depth, 1);
			current_sandbox_trap(WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY);
		} else {
			panic("Runtime SIGSEGV\n");
		}

		break;
	}
	default: {
		const char *signal_name = strsignal(signal_type);
		switch (signal_info->si_code) {
		case SI_TKILL:
			panic("software_interrupt_handle_signals unexpectedly received signal %s from a thread kill\n",
			      signal_name);
		case SI_KERNEL:
			panic("software_interrupt_handle_signals unexpectedly received signal %s from the kernel\n",
			      signal_name);
		default:
			panic("software_interrupt_handle_signals unexpectedly received signal %s with si_code %d\n",
			      signal_name, signal_info->si_code);
		}
	}
	}

	atomic_fetch_sub(&handler_depth, 1);
	return;
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

	/* All supported signals trigger the same signal handler */
	signal_action.sa_sigaction = software_interrupt_handle_signals;
	signal_action.sa_flags     = SA_SIGINFO | SA_RESTART;

	/* Mask SIGALRM and SIGUSR1 while the signal handler executes */
	sigemptyset(&signal_action.sa_mask);
	sigaddset(&signal_action.sa_mask, SIGALRM);
	sigaddset(&signal_action.sa_mask, SIGUSR1);
	sigaddset(&signal_action.sa_mask, SIGFPE);
	sigaddset(&signal_action.sa_mask, SIGSEGV);

	const int    supported_signals[]   = { SIGALRM, SIGUSR1, SIGFPE, SIGSEGV };
	const size_t supported_signals_len = 4;

	for (int i = 0; i < supported_signals_len; i++) {
		int signal = supported_signals[i];

		/* Mask this signal on the listener thread */
		software_interrupt_mask_signal(signal);

		/* But register the handler for this signal for the process */
		if (sigaction(signal, &signal_action, NULL)) {
			perror("sigaction");
			exit(1);
		}
	}

	software_interrupt_counts_alloc();
}

void
software_interrupt_cleanup()
{
	software_interrupt_counts_deferred_sigalrm_max_update(deferred_sigalrm);
	software_interrupt_counts_log();
}
