#pragma once

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#include "software_interrupt.h"

/*
 * This file contains the common dependencies of the architecture-dependent code.
 *
 * While all of the content in this file could alternatively be placed in context.h
 * above the conditional preprocessor includes, IDEs generally assume each file includes
 * their own dependent headers directly and form a clean independent subtree that
 * can be walked to resolve all symbols when the file is active
 */

typedef uint64_t reg_t;

/* Register context saved and restored on user-level, direct context switches. */
typedef enum
{
	UREG_SP = 0,
	UREG_IP = 1,
	UREG_COUNT
} ureg_t;

/* The enum is compared directly in assembly, so maintain integral values! */
typedef enum
{
	ARCH_CONTEXT_VARIANT_UNUSED  = 0, /* Has not have saved a context */
	ARCH_CONTEXT_VARIANT_FAST    = 1, /* Saved a fastpath context */
	ARCH_CONTEXT_VARIANT_SLOW    = 2, /* Saved a slowpath context */
	ARCH_CONTEXT_VARIANT_RUNNING = 3  /* Context is executing and content is out of date */
} arch_context_variant_t;

static inline char *
arch_context_variant_print(arch_context_variant_t context)
{
	switch (context) {
	case ARCH_CONTEXT_VARIANT_UNUSED:
		return "Unused";
	case ARCH_CONTEXT_VARIANT_FAST:
		return "Fast";
	case ARCH_CONTEXT_VARIANT_SLOW:
		return "Slow";
	case ARCH_CONTEXT_VARIANT_RUNNING:
		return "Running";
	default:
		panic("Encountered unexpected arch_context variant: %u\n", context);
	}
}


struct arch_context {
	arch_context_variant_t variant;
	reg_t                  regs[UREG_COUNT];
	mcontext_t             mctx;
};

/*
 * This is the slowpath switch to a preempted sandbox!
 * SIGUSR1 on the current thread and restore mcontext there!
 */
extern __thread struct arch_context worker_thread_base_context;

/* Cannot be inlined because called in assembly */
void __attribute__((noinline)) __attribute__((noreturn)) arch_context_restore_preempted(void);
