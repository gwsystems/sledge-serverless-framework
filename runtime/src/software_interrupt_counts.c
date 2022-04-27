#include "software_interrupt_counts.h"
#include "pretty_print.h"

_Atomic volatile sig_atomic_t *software_interrupt_counts_deferred_sigalrm_max;
_Atomic volatile sig_atomic_t *software_interrupt_counts_deferred_sigalrm_replay;
_Atomic volatile sig_atomic_t *software_interrupt_counts_sigalrm_kernel;
_Atomic volatile sig_atomic_t *software_interrupt_counts_sigalrm_thread;
_Atomic volatile sig_atomic_t *software_interrupt_counts_sigusr;

void
software_interrupt_counts_log()
{
#ifdef LOG_SOFTWARE_INTERRUPT_COUNTS
	for (int i = 0; i < runtime_worker_threads_count; i++) {
		printf("Worker %d:\n", i);
		pretty_print_key_value("Deferred Sigalrm Max", "%12d\n",
		                       software_interrupt_counts_deferred_sigalrm_max[i]);
		pretty_print_key_value("Deferred Sigalrm Replay", "%12d\n",
		                       software_interrupt_counts_deferred_sigalrm_replay[i]);
		pretty_print_key_value("Siglarm Kernel Count", "%12d\n", software_interrupt_counts_sigalrm_kernel[i]);
		pretty_print_key_value("Siglarm Thread Count", "%12d\n", software_interrupt_counts_sigalrm_thread[i]);
		pretty_print_key_value("Sigusr Count", "%12d\n", software_interrupt_counts_sigusr[i]);
	}
	fflush(stdout);
#endif
}
