#pragma once

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#include "software_interrupt.h"

/*
 * This file contains the common dependencies of the architecture-dependent code
 */

typedef uint64_t reg_t;

/* Userspace Registers. */
enum UREGS
{
	UREG_RSP = 0,
	UREG_RIP = 1,
	UREG_COUNT
};

struct arch_context {
	reg_t      regs[UREG_COUNT];
	mcontext_t mctx;
};

/*
 * This is the slowpath switch to a preempted sandbox!
 * SIGUSR1 on the current thread and restore mcontext there!
 */
extern __thread struct arch_context worker_thread_base_context;

/* Cannot be inlined because called in Assembly */
void __attribute__((noinline)) __attribute__((noreturn)) arch_context_mcontext_restore(void);
