#pragma once

#include <assert.h>
#include <errno.h>
#include <panic.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "debuglog.h"

#define SOFTWARE_INTERRUPT_TIME_TO_START_IN_USEC     (2 * 1000) /* 2 ms */
#define SOFTWARE_INTERRUPT_INTERVAL_DURATION_IN_USEC (1 * 1000) /* 1 ms */

/************
 * Externs  *
 ***********/

extern __thread volatile sig_atomic_t software_interrupt_is_disabled;
extern uint64_t                       software_interrupt_interval_duration_in_cycles;

/*************************
 * Public Static Inlines *
 ************************/

static inline void
software_interrupt_disable(void)
{
	if (__sync_bool_compare_and_swap(&software_interrupt_is_disabled, 0, 1) == false) {
		panic("Recursive call to software_interrupt_disable\n");
	}
}


/**
 * Enables signals
 */
static inline void
software_interrupt_enable(void)
{
	if (__sync_bool_compare_and_swap(&software_interrupt_is_disabled, 1, 0) == false) {
		panic("Recursive call to software_interrupt_enable\n");
	}
}

/**
 * @returns boolean if signals are enabled
 */
static inline int
software_interrupt_is_enabled(void)
{
	return (software_interrupt_is_disabled == 0);
}

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

	assert(signal == SIGALRM || signal == SIGUSR1);
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

	assert(signal == SIGALRM || signal == SIGUSR1);
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

/*************************
 * Exports from module.c *
 ************************/

void software_interrupt_initialize(void);
void software_interrupt_arm_timer(void);
void software_interrupt_disarm_timer(void);
