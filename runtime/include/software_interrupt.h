#pragma once

#include <assert.h>
#include <errno.h>
#include <panic.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include "debuglog.h"
#include "runtime.h"
#include "software_interrupt_counts.h"
#include "worker_thread.h"

/*************************
 * Public Static Inlines *
 ************************/

/**
 * Masks a signal on the current thread
 * @param signal - the signal you want to mask
 * @return 0 on success. Exits program otherwise
 */
static inline int
software_interrupt_mask_signal(int signal)
{
	sigset_t set;
	int      return_code;

	assert(signal == SIGALRM || signal == SIGUSR1 || signal == SIGFPE || signal == SIGSEGV || signal == SIGINT);
	/* all threads created by the calling thread will have signal blocked */
	sigemptyset(&set);
	sigaddset(&set, signal);
	return_code = pthread_sigmask(SIG_BLOCK, &set, NULL);
	if (return_code != 0) {
		debuglog("pthread_sigmask: %s", strerror(return_code));
		exit(-1);
	}

	return 0;
}

/**
 * Unmasks a signal on the current thread
 * @param signal - the signal you want to unmask
 * @return 0 on success. Exits program otherwise
 */
static inline int
software_interrupt_unmask_signal(int signal)
{
	sigset_t set;
	int      return_code;

	assert(signal == SIGALRM || signal == SIGUSR1 || signal == SIGFPE || signal == SIGSEGV || signal == SIGINT);
	/* all threads created by the calling thread will have signal unblocked */
	sigemptyset(&set);
	sigaddset(&set, signal);
	return_code = pthread_sigmask(SIG_UNBLOCK, &set, NULL);
	if (return_code != 0) {
		errno = return_code;
		perror("pthread_sigmask");
		exit(-1);
	}

	return 0;
}

extern thread_local _Atomic volatile sig_atomic_t deferred_sigalrm;

static inline void
software_interrupt_deferred_sigalrm_clear()
{
	software_interrupt_counts_deferred_sigalrm_max_update(deferred_sigalrm);
	atomic_store(&deferred_sigalrm, 0);
}

static inline void
software_interrupt_deferred_sigalrm_replay()
{
	if (deferred_sigalrm > 0) {
		software_interrupt_deferred_sigalrm_clear();
		software_interrupt_counts_deferred_sigalrm_replay_increment();
		raise(SIGALRM);
	}
}

/*************************
 * Exports from software_interrupt.c *
 ************************/

void software_interrupt_arm_timer(void);
void software_interrupt_cleanup(void);
void software_interrupt_disarm_timer(void);
void software_interrupt_initialize(void);
