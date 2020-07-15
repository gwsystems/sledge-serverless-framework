#pragma once

/*
 * This header is the single entry point into the arch_context code.
 * This includes both dependent and independent code
 */

/*
 * Conditionally load architecture-dependent code
 * Each of these headers include common.h
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
 * Preempt the current sandbox and start executing the next sandbox
 * @param mc - the context of the current thread of execution
 * @param ctx - the context that we want to restore
 * @return Return code in {0,1}
 * 0 = context restored successfully.
 * 1 = special processing because thread was last in a user-level context switch state
 */
static inline int
arch_mcontext_restore(mcontext_t *mc, struct arch_context *ctx)
{
	assert(ctx != &worker_thread_base_context);
	assert(!software_interrupt_is_enabled());

	/* if ctx->regs[0] is set, this was last in a user-level context switch state!
	 * else restore mcontext..
	 */
	bool did_user_level_context_switch = ctx->regs[UREG_RSP];
	if (did_user_level_context_switch) {
		mc->gregs[REG_RSP]  = ctx->regs[UREG_RSP];
		mc->gregs[REG_RIP]  = ctx->regs[UREG_RIP] + ARCH_SIG_JMP_OFF;
		ctx->regs[UREG_RSP] = 0;

		return 1;
	}

	/* Restore mcontext */
	memcpy(mc, &ctx->mctx, sizeof(mcontext_t));
	memset(&ctx->mctx, 0, sizeof(mcontext_t));

	return 0;
}

/**
 * Save the context of the currently executing process
 * @param ctx - destination
 * @param mc - source
 */
static inline void
arch_mcontext_save(struct arch_context *ctx, mcontext_t *mc)
{
	assert(ctx != &worker_thread_base_context);

	ctx->regs[UREG_RSP] = 0;
	memcpy(&ctx->mctx, mc, sizeof(mcontext_t));
}
