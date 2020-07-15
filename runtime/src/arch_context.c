#include <signal.h>
#include <pthread.h>

#include "types.h"

/**
 * Called by the inline assembly in arch_context_switch to send a SIGUSR1 in order to restore a previously preempted
 * thread. The only way to restore all of the mcontext registers of a preempted sandbox is to send ourselves a signal,
 * then update the registers we should return to, then sigreturn (by returning from the handler). This returns to the
 * control flow restored from the mcontext
 */
void __attribute__((noinline)) __attribute__((noreturn)) arch_context_mcontext_restore(void)
{
	debuglog("Sending SIGUSR1");
	pthread_kill(pthread_self(), SIGUSR1);
	assert(false);
}
