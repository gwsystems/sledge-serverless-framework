#pragma once
#include "software_interrupt.h"
#include "worker_thread.h"

// Broken into a distict header because of circular dependency issues

/**
 * Reenables signals, replaying deferred signals as needed
 */
static inline void
software_interrupt_enable(void)
{
	/* Trigger missed SIGALRM */
	if (software_interrupt_deferred_sigalrm > 0) {
		debuglog("Missed %d sigalrms, resetting\n", software_interrupt_deferred_sigalrm);
		software_interrupt_deferred_sigalrm = 0;

		worker_thread_sched();
	}

	if (__sync_bool_compare_and_swap(&software_interrupt_is_disabled, 1, 0) == false) {
		panic("Recursive call to software_interrupt_enable\n");
	}
}
