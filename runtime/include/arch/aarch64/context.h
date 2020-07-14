#pragma once

#include <stdint.h>
#include <unistd.h>
#include <ucontext.h>

#define ARCH_NREGS       (2)   /* SP + PC only */
#define ARCH_SIG_JMP_OFF 0x100 /* Based on code generated! */

/**
 * ARM64 code. Currently Unimplemented
 */

typedef uint64_t reg_t;

struct arch_context {
	reg_t      regs[ARCH_NREGS];
	mcontext_t mctx;
};

extern void __attribute__((noreturn)) worker_thread_mcontext_restore(void);
extern __thread struct arch_context worker_thread_base_context;

/* Initialized a context, zeroing out registers and setting the Instruction and Stack pointers */
static inline void
arch_context_init(struct arch_context *actx, reg_t ip, reg_t sp)
{
	memset(&actx->mctx, 0, sizeof(mcontext_t));
	memset((void *)actx->regs, 0, sizeof(reg_t) * ARCH_NREGS);

	*(actx->regs)     = sp;
	*(actx->regs + 1) = ip;
}

static int
arch_mcontext_restore(mcontext_t *mc, struct arch_context *ctx)
{
	assert(ctx != &worker_thread_base_context);

	/*
	 * if ctx->regs[0] is set, this was last in a user-level context switch state!
	 * else restore mcontext..
	 */
	if (ctx->regs[0]) {
		mc->sp       = ctx->regs[0];
		mc->pc       = ctx->regs[1] + ARCH_SIG_JMP_OFF;
		ctx->regs[0] = 0;

		return 1;
	} else {
		memcpy(mc, &ctx->mctx, sizeof(mcontext_t));
		memset(&ctx->mctx, 0, sizeof(mcontext_t));
	}

	return 0;
}

static void
arch_mcontext_save(struct arch_context *ctx, mcontext_t *mc)
{
	assert(ctx != &worker_thread_base_context);

	ctx->regs[0] = 0;
	memcpy(&ctx->mctx, mc, sizeof(mcontext_t));
}


static inline int
arch_context_switch(struct arch_context *ca, struct arch_context *na)
{
	if (!ca) {
		assert(na);
		/* switching from "no sandbox to execute" state to "executing a sandbox" */
		ca = &worker_thread_base_context;
	} else if (!na) {
		assert(ca);

		/* switching from "executing a sandbox" to "no execution" state. */
		na = &worker_thread_base_context;
	} else {
		assert(na && ca);

		/* switching between sandboxes. */
	}

	reg_t *cr = ca->regs, *nr = na->regs;
	assert(cr && nr);

	asm volatile("mov x0, sp\n\t"
	             "adr x1, reset%=\n\t"
	             "str x1, [%[curr], 8]\n\t"
	             "str x0, [%[curr]]\n\t"
	             "ldr x2, [%[next]]\n\t"
	             "cbz x2, slow%=\n\t"
	             "ldr x3, [%[next], 8]\n\t"
	             "mov sp, x2\n\t"
	             "br x3\n\t"
	             "slow%=:\n\t"
	             "br %[slowpath]\n\t"
	             ".align 8\n\t"
	             "reset%=:\n\t"
	             "mov x1, #0\n\t"
	             "str x1, [%[next]]\n\t"
	             ".align 8\n\t"
	             "exit%=:\n\t"
	             :
	             : [ curr ] "r"(cr), [ next ] "r"(nr), [ slowpath ] "r"(&worker_thread_mcontext_restore)
	             : "memory", "cc", "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12",
	               "x13", "x14", "x15", "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25", "x26",
	               "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15");
	return 0;
}
