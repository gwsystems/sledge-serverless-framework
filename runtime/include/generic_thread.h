#pragma once

#include <stdint.h>
#include <threads.h>

extern thread_local uint64_t    generic_thread_lock_duration;
extern thread_local uint64_t    generic_thread_lock_longest;
extern thread_local const char *generic_thread_lock_longest_fn;
extern thread_local uint64_t    generic_thread_start_timestamp;

void generic_thread_dump_lock_overhead(void);
void generic_thread_initialize(void);
