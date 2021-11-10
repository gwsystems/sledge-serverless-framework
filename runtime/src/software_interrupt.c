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
#include "scheduler.h"
#include "software_interrupt.h"
#include "memlogging.h"

/*******************
 * Process Globals *
 ******************/

static uint64_t software_interrupt_interval_duration_in_cycles;

/******************
 * Thread Globals *
 *****************/

__thread _Atomic static volatile sig_atomic_t software_interrupt_SIGALRM_kernel_count = 0;
__thread _Atomic static volatile sig_atomic_t software_interrupt_SIGALRM_thread_count = 0;
__thread _Atomic static volatile sig_atomic_t software_interrupt_SIGUSR_count         = 0;
__thread _Atomic volatile sig_atomic_t        software_interrupt_deferred_sigalrm     = 0;
__thread _Atomic volatile sig_atomic_t        software_interrupt_signal_depth         = 0;

_Atomic volatile sig_atomic_t software_interrupt_deferred_sigalrm_max[RUNTIME_WORKER_THREAD_CORE_COUNT] = { 0 };

extern pthread_t listener_thread_id;
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
				assert(scheduler == SCHEDULER_EDF);
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
 * A POSIX signal is delivered to only one thread.
 * This function broadcasts the sigint signal to all other worker threads
 */
static inline void
sigint_propagate_workers_listener(siginfo_t *signal_info)
{
	/* Signal was sent directly by the kernel user space, so forward to other threads */
	if (signal_info->si_code == SI_KERNEL || signal_info->si_code == SI_USER) {
		for (int i = 0; i < runtime_worker_threads_count; i++) {
			if (pthread_self() == runtime_worker_threads[i]) continue;

			/* All threads should have been initialized */
			assert(runtime_worker_threads[i] != 0);
			pthread_kill(runtime_worker_threads[i], SIGINT);
		}
		/* send to listener thread */
		if (pthread_self() != listener_thread_id) {
			pthread_kill(listener_thread_id, SIGINT);
		}		
	} else {
		/* Signal forwarded from another thread. Just confirm it resulted from pthread_kill */
		assert(signal_info->si_code == SI_TKILL);
	}
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
 * SIGINT recieved by a worker threads and propagate to other worker threads. When one worker thread
 *        receives a SIGINT that is not from kernel, dump all memory log to a file and then exit.
 * @param signal_type
 * @param signal_info data structure containing signal info
 * @param user_context_raw void* to a user_context struct
 */
static inline void
software_interrupt_handle_signals(int signal_type, siginfo_t *signal_info, void *user_context_raw)
{
	/* Only workers should receive signals */
	assert(!listener_thread_is_running());

	/* Signals should be masked if runtime has disabled them */
	/* This is not applicable anymore because the signal might be SIGINT */
	//assert(runtime_preemption_enabled);

	/* Signals should not nest */
	/* TODO: Better atomic instruction here to check and set? */
	assert(software_interrupt_signal_depth == 0);
	atomic_fetch_add(&software_interrupt_signal_depth, 1);

	ucontext_t *    user_context    = (ucontext_t *)user_context_raw;
	struct sandbox *current_sandbox = current_sandbox_get();

	switch (signal_type) {
	case SIGALRM: {
		sigalrm_propagate_workers(signal_info);
		if (current_sandbox == NULL || current_sandbox->ctxt.preemptable == false) {
			/* Cannot preempt, so defer signal
			 * TODO: First worker gets tons of kernel sigalrms, should these be treated the same?
			 * When current_sandbox is NULL, we are looping through the scheduler, so sigalrm is redundant
			 * Maybe track time of last scheduling decision? i.e. when scheduler_get_next was last called.
			 */
			atomic_fetch_add(&software_interrupt_deferred_sigalrm, 1);
		} else {
			/* A worker thread received a SIGALRM while running a preemptable sandbox, so preempt */
			//assert(current_sandbox->state == SANDBOX_RUNNING);
			scheduler_preempt(user_context);
		}
		goto done;
	}
	case SIGUSR1: {
		assert(current_sandbox);
		assert(current_sandbox->ctxt.variant == ARCH_CONTEXT_VARIANT_SLOW);
		atomic_fetch_add(&software_interrupt_SIGUSR_count, 1);
#ifdef LOG_PREEMPTION
		debuglog("Total SIGUSR1 Received: %d\n", software_interrupt_SIGUSR_count);
		debuglog("Restoring sandbox: %lu, Stack %llu\n", current_sandbox->id,
		         current_sandbox->ctxt.mctx.gregs[REG_RSP]);
#endif
		uint64_t now = __getcycles();
		current_sandbox->last_state_change_timestamp = now;
		arch_mcontext_restore(&user_context->uc_mcontext, &current_sandbox->ctxt);
		goto done;
	}
	case SIGINT: {
		/* Only the thread that receives SIGINT from the kernel or user space will broadcast SIGINT to other worker threads */
		sigint_propagate_workers_listener(signal_info);
		dump_log_to_file();
		/* terminate itself */	
		pthread_exit(0);	
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
done:
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
 * Register softint_handler to execute on SIGALRM, SIGINT, and SIGUSR1
 */
void
software_interrupt_initialize(void)
{
	struct sigaction signal_action;
	memset(&signal_action, 0, sizeof(struct sigaction));
	signal_action.sa_sigaction = software_interrupt_handle_signals;
	signal_action.sa_flags     = SA_SIGINFO | SA_RESTART;

	/* all threads created by the calling thread will have signal blocked */
	/* TODO: What does sa_mask do? I have to call software_interrupt_mask_signal below */
	sigemptyset(&signal_action.sa_mask);
	sigaddset(&signal_action.sa_mask, SIGALRM);
	sigaddset(&signal_action.sa_mask, SIGUSR1);
	sigaddset(&signal_action.sa_mask, SIGINT);

	const int    supported_signals[]   = { SIGALRM, SIGUSR1, SIGINT };
	const size_t supported_signals_len = 3;

	for (int i = 0; i < supported_signals_len; i++) {
		int signal = supported_signals[i];
		software_interrupt_mask_signal(signal);
		if (sigaction(signal, &signal_action, NULL)) {
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
