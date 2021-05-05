#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#include "arch/arch_context_t.h"
#include "arch/arch_context_variant_t.h"
#include "arch/reg_t.h"
#include "arch/ureg_t.h"
#include "software_interrupt.h"

/*
 * This file contains the common dependencies of the architecture-dependent code.
 *
 * While all of the content in this file could alternatively be placed in context.h
 * above the conditional preprocessor includes, IDEs generally assume each file includes
 * their own dependent headers directly and form a clean independent subtree that
 * can be walked to resolve all symbols when the file is active
 */


/*
 * This is the slowpath switch to a preempted sandbox!
 * SIGUSR1 on the current thread and restore mcontext there!
 */
extern __thread struct arch_context worker_thread_base_context;

/* Cannot be inlined because called in assembly */
void __attribute__((noinline)) __attribute__((noreturn)) arch_context_restore_preempted(void);
