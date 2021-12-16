#pragma once

#include <stdint.h>
#include <stdio.h>
#include <threads.h>

/* For this family of macros, do NOT pass zero as the pow2 */
#define round_to_pow2(x, pow2)    (((unsigned long)(x)) & (~((pow2)-1)))
#define round_up_to_pow2(x, pow2) (round_to_pow2(((unsigned long)(x)) + (pow2)-1, (pow2)))

#define round_to_page(x)    round_to_pow2(x, PAGE_SIZE)
#define round_up_to_page(x) round_up_to_pow2(x, PAGE_SIZE)

#define EXPORT       __attribute__((visibility("default")))
#define IMPORT       __attribute__((visibility("default")))
#define INLINE       __attribute__((always_inline))
#define PAGE_ALIGNED __attribute__((aligned(PAGE_SIZE)))
#define PAGE_SIZE    (unsigned long)(1 << 12)
#define WEAK         __attribute__((weak))
#define CACHE_LINE   64

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
