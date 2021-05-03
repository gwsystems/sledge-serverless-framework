#pragma once

#include <stdint.h>

#include "arch/getcycles.h"
#include "debuglog.h"

extern __thread uint64_t generic_thread_lock_duration;
extern __thread uint64_t generic_thread_start_timestamp;

/**
 * Reports lock contention
 */
static inline void
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

void generic_thread_initialize();
