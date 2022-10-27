#pragma once

#include <stdatomic.h>
#include <signal.h>
#include <stdlib.h>

#include "worker_thread.h"

extern _Atomic volatile sig_atomic_t *software_interrupt_counts_deferred_sigalrm_max;
extern _Atomic volatile sig_atomic_t *software_interrupt_counts_deferred_sigalrm_replay;
extern _Atomic volatile sig_atomic_t *software_interrupt_counts_sigalrm_kernel;
extern _Atomic volatile sig_atomic_t *software_interrupt_counts_sigalrm_thread;
extern _Atomic volatile sig_atomic_t *software_interrupt_counts_sigfpe;
extern _Atomic volatile sig_atomic_t *software_interrupt_counts_sigsegv;
extern _Atomic volatile sig_atomic_t *software_interrupt_counts_sigusr;

static inline void
software_interrupt_counts_alloc()
{
#ifdef LOG_SOFTWARE_INTERRUPT_COUNTS
	software_interrupt_counts_deferred_sigalrm_max    = calloc(runtime_worker_threads_count, sizeof(sig_atomic_t));
	software_interrupt_counts_deferred_sigalrm_replay = calloc(runtime_worker_threads_count, sizeof(sig_atomic_t));
	software_interrupt_counts_sigalrm_kernel          = calloc(runtime_worker_threads_count, sizeof(sig_atomic_t));
	software_interrupt_counts_sigalrm_thread          = calloc(runtime_worker_threads_count, sizeof(sig_atomic_t));
	software_interrupt_counts_sigfpe                  = calloc(runtime_worker_threads_count, sizeof(sig_atomic_t));
	software_interrupt_counts_sigsegv                 = calloc(runtime_worker_threads_count, sizeof(sig_atomic_t));
	software_interrupt_counts_sigusr                  = calloc(runtime_worker_threads_count, sizeof(sig_atomic_t));
#endif
}

static inline void
software_interrupt_counts_free()
{
#ifdef LOG_SOFTWARE_INTERRUPT_COUNTS
	free((void *)software_interrupt_counts_deferred_sigalrm_max);
	free((void *)software_interrupt_counts_deferred_sigalrm_replay);
	free((void *)software_interrupt_counts_sigalrm_kernel);
	free((void *)software_interrupt_counts_sigalrm_thread);
	free((void *)software_interrupt_counts_sigfpe);
	free((void *)software_interrupt_counts_sigsegv);
	free((void *)software_interrupt_counts_sigusr);
#endif
}

static inline void
software_interrupt_counts_deferred_sigalrm_max_update(int deferred_sigalrm_count)
{
#ifdef LOG_SOFTWARE_INTERRUPT_COUNTS
	if (unlikely(deferred_sigalrm_count > software_interrupt_counts_deferred_sigalrm_max[worker_thread_idx])) {
		software_interrupt_counts_deferred_sigalrm_max[worker_thread_idx] = deferred_sigalrm_count;
	}
#endif
}

static inline void
software_interrupt_counts_deferred_sigalrm_replay_increment()
{
#ifdef LOG_SOFTWARE_INTERRUPT_COUNTS
	//atomic_fetch_add(&software_interrupt_counts_deferred_sigalrm_replay[worker_thread_idx], 1);
#endif
}

static inline void
software_interrupt_counts_sigalrm_kernel_increment()
{
#ifdef LOG_SOFTWARE_INTERRUPT_COUNTS
	//atomic_fetch_add(&software_interrupt_counts_sigalrm_kernel[worker_thread_idx], 1);
#endif
}

static inline void
software_interrupt_counts_sigalrm_thread_increment()
{
#ifdef LOG_SOFTWARE_INTERRUPT_COUNTS
	//atomic_fetch_add(&software_interrupt_counts_sigalrm_thread[worker_thread_idx], 1);
#endif
}

static inline void
software_interrupt_counts_sigfpe_increment()
{
#ifdef LOG_SOFTWARE_INTERRUPT_COUNTS
	//atomic_fetch_add(&software_interrupt_counts_sigfpe[worker_thread_idx], 1);
#endif
}

static inline void
software_interrupt_counts_sigsegv_increment()
{
#ifdef LOG_SOFTWARE_INTERRUPT_COUNTS
	//atomic_fetch_add(&software_interrupt_counts_sigsegv[worker_thread_idx], 1);
#endif
}

static inline void
software_interrupt_counts_sigusr_increment()
{
#ifdef LOG_SOFTWARE_INTERRUPT_COUNTS
	//atomic_fetch_add(&software_interrupt_counts_sigusr[worker_thread_idx], 1);
#endif
}

extern void software_interrupt_counts_log();
