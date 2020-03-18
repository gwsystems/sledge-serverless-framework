#ifndef SFRT_SOFTWARE_INTERRUPT_H
#define SFRT_SOFTWARE_INTERRUPT_H

#include <stdbool.h>
#include <assert.h>
#include <signal.h>

/***************************************
 * Externs
 ***************************************/

extern __thread volatile sig_atomic_t software_interrupt__is_disabled;

/***************************************
 * Public Static Inlines
 ***************************************/

static inline void
software_interrupt__disable(void)
{
	while (__sync_bool_compare_and_swap(&software_interrupt__is_disabled, 0, 1) == false)
		;
}


/**
 * Enables signals
 */
static inline void
software_interrupt__enable(void)
{
	if (__sync_bool_compare_and_swap(&software_interrupt__is_disabled, 1, 0) == false) assert(0);
}

/**
 * @returns boolean if signals are enabled
 */
static inline int
software_interrupt__is_enabled(void)
{
	return (software_interrupt__is_disabled == 0);
}

/**
 * Masks a signal on the current thread
 * @param signal - the signal you want to mask
 * @return 0 on success. Exits program otherwise
 **/
static inline int
software_interrupt__mask_signal(int signal)
{
	sigset_t set;
	int      return_code;

	assert(signal == SIGALRM || signal == SIGUSR1);
	/* all threads created by the calling thread will have signal blocked */
	sigemptyset(&set);
	sigaddset(&set, signal);
	return_code = pthread_sigmask(SIG_BLOCK, &set, NULL);
	if (return_code != 0) {
		errno = return_code;
		perror("pthread_sigmask");
		exit(-1);
	}

	return 0;
}

/**
 * Unmasks a signal on the current thread
 * @param signal - the signal you want to unmask
 * @return 0 on success. Exits program otherwise
 **/
static inline int
software_interrupt__unmask_signal(int signal)
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

/***************************************
 * Exports from module.c
 ***************************************/

void software_interrupt__initialize(void);
void software_interrupt__arm_timer(void);
void software_interrupt__disarm_timer(void);

#endif /* SFRT_SOFTWARE_INTERRUPT_H */
