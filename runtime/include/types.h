#pragma once

#include <stdint.h>
#include <stdio.h>
#include <threads.h>

#define PAGE_SIZE  (unsigned long)(1 << 12)
#define CACHE_LINE 64
/* This might be Intel specific. ARM and x64 both have the same CACHE_LINE size, but x64 uses Intel uses a double
 * cache-line as a coherency unit */
#define CACHE_PAD (CACHE_LINE * 2)

/* For this family of macros, do NOT pass zero as the pow2 */
#define round_to_pow2(x, pow2) (((unsigned long)(x)) & (~(((unsigned long)(pow2)) - 1)))
#define round_up_to_pow2(x, pow2) \
	(round_to_pow2(((unsigned long)(x)) + ((unsigned long)(pow2)) - 1, (unsigned long)((pow2))))

#define round_to_page(x)    round_to_pow2(x, PAGE_SIZE)
#define round_up_to_page(x) round_up_to_pow2(x, PAGE_SIZE)

#define round_to_cache_pad(x)    round_to_pow2((unsigned long)(x), CACHE_PAD)
#define round_up_to_cache_pad(x) round_up_to_pow2((unsigned long)(x), CACHE_PAD)

#define PAGE_ALIGNED      __attribute__((aligned(PAGE_SIZE)))
#define CACHE_PAD_ALIGNED __attribute__((aligned(CACHE_PAD)))

#define EXPORT __attribute__((visibility("default")))
#define IMPORT __attribute__((visibility("default")))
#define INLINE __attribute__((always_inline))
#define WEAK   __attribute__((weak))


#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
