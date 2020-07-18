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

/* Userspace Registers. */
enum UREGS
{
	UREG_RSP = 0,
	UREG_RIP = 1,
	UREG_COUNT
};

/* The enum is compared directly in assembly, so maintain integral values! */
enum ARCH_CONTEXT
{
	ARCH_CONTEXT_UNUSED  = 0,
	ARCH_CONTEXT_FAST    = 1,
	ARCH_CONTEXT_SLOW    = 2,
	ARCH_CONTEXT_RUNNING = 3
};


struct arch_context {
	enum ARCH_CONTEXT variant;
	reg_t             regs[UREG_COUNT];
	mcontext_t        mctx;
};

/*
 * This is the slowpath switch to a preempted sandbox!
 * SIGUSR1 on the current thread and restore mcontext there!
 */
extern __thread struct arch_context worker_thread_base_context;

/* Cannot be inlined because called in assembly */
void __attribute__((noinline)) __attribute__((noreturn)) arch_context_mcontext_restore(void);

extern __thread volatile bool worker_thread_is_switching_context;
