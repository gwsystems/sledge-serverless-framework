#ifndef SFRT_SOFTINT_H
#define SFRT_SOFTINT_H

#include <stdbool.h>
#include <assert.h>
#include <signal.h>

static inline int
softint_mask(int signal)
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

static inline int
softint_unmask(int signal)
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

extern __thread volatile sig_atomic_t softint_off;

static inline void
softint_disable(void)
{
	while (__sync_bool_compare_and_swap(&softint_off, 0, 1) == false)
		;
}

static inline void
softint_enable(void)
{
	if (__sync_bool_compare_and_swap(&softint_off, 1, 0) == false) assert(0);
}

static inline int
softint_enabled(void)
{
	return (softint_off == 0);
}

void softint_init(void);

void softint_timer_arm(void);
void softint_timer_disarm(void);

#endif /* SFRT_SOFTINT_H */
