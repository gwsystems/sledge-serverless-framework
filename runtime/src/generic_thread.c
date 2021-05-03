#include <stdint.h>

#include "arch/getcycles.h"

/* Implemented by listener and workers */

__thread uint64_t generic_thread_lock_duration   = 0;
__thread uint64_t generic_thread_start_timestamp = 0;

void
generic_thread_initialize()
{
	generic_thread_start_timestamp = __getcycles();
	generic_thread_lock_duration   = 0;
}
