#include <stdint.h>

#include "arch/getcycles.h"
#include "debuglog.h"

/* Implemented by listener and workers */

__thread uint64_t generic_thread_lock_duration   = 0;
__thread uint64_t generic_thread_start_timestamp = 0;

void
generic_thread_initialize()
{
	generic_thread_start_timestamp = __getcycles();
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
#endif
#endif
}
