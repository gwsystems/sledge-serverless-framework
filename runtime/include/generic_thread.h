#pragma once

#include <stdint.h>

extern __thread uint64_t generic_thread_lock_duration;
extern __thread uint64_t generic_thread_lock_longest;
extern __thread uint64_t generic_thread_start_timestamp;

void generic_thread_dump_lock_overhead(void);
void generic_thread_initialize(void);
