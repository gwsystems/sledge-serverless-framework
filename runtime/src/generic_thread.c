#include <stdint.h>

#include "arch/getcycles.h"
#include "debuglog.h"

extern uint32_t                     runtime_processor_speed_MHz;
extern uint32_t                     runtime_quantum_us;

/* Implemented by listener and workers */

__thread uint64_t generic_thread_lock_duration   = 0;
__thread uint64_t generic_thread_lock_longest    = 0;
__thread uint64_t generic_thread_start_timestamp = 0;

void
generic_thread_initialize()
{
	generic_thread_start_timestamp = __getcycles();
	generic_thread_lock_longest    = 0;
	generic_thread_lock_duration   = 0;
}

/**
 * Reports lock contention
 */
void
generic_thread_dump_lock_overhead()
{
#ifndef NDEBUG
#ifdef LOG_LOCK_OVERHEAD
	uint64_t duration = __getcycles() - generic_thread_start_timestamp;
	debuglog("Locks consumed %lu / %lu cycles, or %f%%\n", generic_thread_lock_duration, duration,
	         (double)generic_thread_lock_duration / duration * 100);
	debuglog("Longest Held Lock was %lu cycles, or %f quantums\n", generic_thread_lock_longest,
	         (double)generic_thread_lock_longest / ((uint64_t)runtime_processor_speed_MHz * runtime_quantum_us));
#endif
#endif
}
