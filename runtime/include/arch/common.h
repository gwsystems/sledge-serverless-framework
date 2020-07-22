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
	ureg_rsp = 0,
	ureg_rip = 1,
	ureg_count
} ureg_t;

/* The enum is compared directly in assembly, so maintain integral values! */
typedef enum
{
	arch_context_variant_unused  = 0, /* Has not have saved a context */
	arch_context_variant_fast    = 1, /* Saved a fastpath context */
	arch_context_variant_slow    = 2, /* Saved a slowpath context */
	arch_context_variant_running = 3  /* Context is executing and content is out of date */
} arch_context_variant_t;

static inline char *
arch_context_variant_print(arch_context_variant_t context)
{
	switch (context) {
	case arch_context_variant_unused:
		return "Unused";
	case arch_context_variant_fast:
		return "Fast";
	case arch_context_variant_slow:
		return "Slow";
	case arch_context_variant_running:
		return "Running";
	default:
		panic("Encountered unexpected arch_context variant\n");
	}
}


struct arch_context {
	arch_context_variant_t variant;
	reg_t                  regs[ureg_count];
	mcontext_t             mctx;
};

/*
 * This is the slowpath switch to a preempted sandbox!
 * SIGUSR1 on the current thread and restore mcontext there!
 */
extern __thread struct arch_context worker_thread_base_context;

/* Cannot be inlined because called in assembly */
void __attribute__((noinline)) __attribute__((noreturn)) arch_context_restore_preempted(void);
