#ifndef ARCH_X86_64_CONTEXT_H
#define ARCH_X86_64_CONTEXT_H

#include <assert.h>
#include <ucontext.h>
#include <unistd.h>

#define ARCH_NREGS       2 // for SP & IP only
#define ARCH_SIG_JMP_OFF 8

typedef uint64_t reg_t;

/*
 * This is the slowpath switch to a preempted sandbox!
 * SIGUSR1 on the current thread and restore mcontext there!
 */

struct arch_context {
	reg_t      regs[ARCH_NREGS];
	mcontext_t mctx;
};

typedef struct arch_context arch_context_t;

extern void __attribute__((noreturn)) worker_thread_sandbox_switch_preempt(void);
extern __thread arch_context_t worker_thread_base_context;

static void __attribute__((noinline)) arch_context_init(arch_context_t *actx, reg_t ip, reg_t sp)
{
	memset(&actx->mctx, 0, sizeof(mcontext_t));
	memset((void *)actx->regs, 0, sizeof(reg_t) * ARCH_NREGS);

	if (sp) {
		/*
		 * context_switch conventions: bp is expected to be on top of the stack
		 * when co-op context switching..
		 *
		 * so push sp on this new stack and use
		 * that new sp as sp for switching to sandbox!
		 */
		asm volatile("movq %%rsp, %%rbx\n\t"
		             "movq %%rax, %%rsp\n\t"
		             "pushq %%rax\n\t"
		             "movq %%rsp, %%rax\n\t"
		             "movq %%rbx, %%rsp\n\t"
		             : "=a"(sp)
		             : "a"(sp)
		             : "memory", "cc", "rbx");
	}

	*(actx->regs)     = sp;
	*(actx->regs + 1) = ip;
}

static int
arch_mcontext_restore(mcontext_t *mc, arch_context_t *ctx)
{
	assert(ctx != &worker_thread_base_context);

	// if ctx->regs[0] is set, this was last in a user-level context switch state!
	// else restore mcontext..
	if (ctx->regs[0]) {
		mc->gregs[REG_RSP] = ctx->regs[0];
		mc->gregs[REG_RIP] = ctx->regs[1] + ARCH_SIG_JMP_OFF;
		ctx->regs[0]       = 0;

		return 1;
	} else {
		memcpy(mc, &ctx->mctx, sizeof(mcontext_t));
		memset(&ctx->mctx, 0, sizeof(mcontext_t));
	}

	return 0;
}

static void
arch_mcontext_save(arch_context_t *ctx, mcontext_t *mc)
{
	assert(ctx != &worker_thread_base_context);

	ctx->regs[0] = 0;
	memcpy(&ctx->mctx, mc, sizeof(mcontext_t));
}


static inline int
arch_context_switch(arch_context_t *ca, arch_context_t *na)
{
	if (!ca) {
		assert(na);
		// switching from "no sandbox to execute" state to "executing a sandbox"
		ca = &worker_thread_base_context;
	} else if (!na) {
		assert(ca);

		// switching from "executing a sandbox" to "no execution" state.
		na = &worker_thread_base_context;
	} else {
		assert(na && ca);

		// switching between sandboxes.
	}

	reg_t *cr = ca->regs, *nr = na->regs;
	assert(cr && nr);

	asm volatile("pushq %%rbp\n\t"
	             "movq %%rsp, %%rbp\n\t"
	             "movq $2f, 8(%%rax)\n\t"
	             "movq %%rsp, (%%rax)\n\t"
	             "cmpq $0, (%%rbx)\n\t"
	             "je 1f\n\t"
	             "movq (%%rbx), %%rsp\n\t"
	             "jmpq *8(%%rbx)\n\t"
	             "1:\n\t"
	             "call worker_thread_sandbox_switch_preempt\n\t"
	             ".align 8\n\t"
	             "2:\n\t"
	             "movq $0, (%%rbx)\n\t"
	             ".align 8\n\t"
	             "3:\n\t"
	             "popq %%rbp\n\t"
	             :
	             : "a"(cr), "b"(nr)
	             : "memory", "cc", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
	               "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11",
	               "xmm12", "xmm13", "xmm14", "xmm15");

	return 0;
}

#endif /* ARCH_X86_64_CONTEXT_H */
