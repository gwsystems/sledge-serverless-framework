#pragma once

#include <stdatomic.h>
#include <stdint.h>

/* Count of the total number of requests we've ever allocated. Never decrements as it is used to generate IDs */
extern _Atomic uint64_t sandbox_total;

static inline void
sandbox_total_initialize()
{
	atomic_init(&sandbox_total, 0);
}

static inline uint64_t
sandbox_total_postfix_increment()
{
	return atomic_fetch_add(&sandbox_total, 1) + 1;
}
