#pragma once

/*
 * This header is the single entry point into the arch_context code.
 * It includes processor independent code and conditionally includes architecture
 * dependent code.
 */

/*
 * These preprocessor conditionals load architecture-dependent code.
 *
 * Each of these headers include common.h, a sibling header that contains common
 * structures, enums, and externs used by the processor independent code. Those
 * definitions are thus transitively loaded here with the appropriate architecture
 * specific code.
 *
 * While the code in common.h could be located in this file above the conditionals,
 * structuring the code as such prevents IDEs from knowing how to resolve the
 * symbols when aarch64/context.h or x86_64/context is the active file.
 */
#if defined(AARCH64) || defined(aarch64)
#include "aarch64/context.h"
#elif defined(X86_64) || defined(x86_64)
#include "x86_64/context.h"
#else
#warning "Architecture not set. Using x86_64"
#define X86_64
#include "x86_64/context.h"
#endif


/**
 * Restore a full mcontext
 * Writes sandbox_context to active_context and then zeroes sandbox_context out
 * @param active_context - the context of the current worker thread
 * @param sandbox_context - the context that we want to restore
 */
static inline void
arch_mcontext_restore(mcontext_t *active_context, struct arch_context *sandbox_context)
{
	assert(!software_interrupt_is_enabled());

	assert(active_context != NULL);
	assert(sandbox_context != NULL);

	/* Validate that the sandbox_context is well formed */
	assert(sandbox_context->variant == arch_context_slow);
	assert(sandbox_context->mctx.gregs[REG_RSP] != 0);
	assert(sandbox_context->mctx.gregs[REG_RIP] != 0);

	assert(sandbox_context != &worker_thread_base_context);

	/* Restore mcontext */
	memcpy(active_context, &sandbox_context->mctx, sizeof(mcontext_t));
	memset(&sandbox_context->mctx, 0, sizeof(mcontext_t));
}

/**
 * Restore a sandbox that was previously executing and preempted for higher-priority work.
 * This method restores only the instruction pointer and stack pointer registers rather than
 * a full mcontext, so it is less expensive than arch_mcontext_restore.
 * @param active_context - the context of the current worker thread
 * @param sandbox_context - the context that we want to restore
 */
static void
arch_context_restore(mcontext_t *active_context, struct arch_context *sandbox_context)
{
	assert(active_context != NULL);
	assert(sandbox_context != NULL);
	assert(sandbox_context->variant == arch_context_fast);
	assert(sandbox_context != &worker_thread_base_context);

	/* TODO: Phani explained that we need to be able to restore a sandbox with an IP of 0. Why is this? */
	assert(sandbox_context->regs[ureg_rsp]);

	active_context->gregs[REG_RSP]  = sandbox_context->regs[ureg_rsp];
	active_context->gregs[REG_RIP]  = sandbox_context->regs[ureg_rip] + ARCH_SIG_JMP_OFF;
	sandbox_context->regs[ureg_rsp] = 0;
	sandbox_context->regs[ureg_rip] = 0;
}

/**
 * Save the full mcontext of the currently executing process
 * @param sandbox_context - destination
 * @param active_context - source
 */
static inline void
arch_mcontext_save(struct arch_context *sandbox_context, const mcontext_t *active_context)
{
	assert(sandbox_context != NULL);
	assert(active_context != NULL);

	/* Assumption: Only called indirectly via signal handler, so interrupts should be disabled */
	assert(!software_interrupt_is_enabled());

	/* Assumption: The base context should never be modified */
	assert(sandbox_context != &worker_thread_base_context);

	/* Assumption: The executing process has sane IP and SP values */
	assert(active_context->gregs[REG_RIP] != 0);
	assert(active_context->gregs[REG_RSP] != 0);

	/* Set variant to slow */
	sandbox_context->variant        = arch_context_slow;
	sandbox_context->regs[ureg_rsp] = 0;
	sandbox_context->regs[ureg_rip] = 0;

	/* Copy mcontext */
	memcpy(&sandbox_context->mctx, active_context, sizeof(mcontext_t));
}
