#ifndef ARCH_AARCH64_CONTEXT_H
#define ARCH_AARCH64_CONTEXT_H

#include <unistd.h>
#include <ucontext.h>

typedef uint64_t reg_t;
#define ARCH_NREGS 31 // TODO: aarch64 context-switch assembly and registers!

/*
 * This is the slowpath switch to a preempted sandbox!
 * SIGUSR1 on the current thread and restore mcontext there!
 */
extern void __attribute__((noreturn)) sandbox_switch_preempt(void);

struct arch_context {
	reg_t      regs[ARCH_NREGS];
	mcontext_t mctx;
};

typedef struct arch_context    arch_context_t;
extern __thread arch_context_t base_context;

static inline void
arch_context_init(arch_context_t *actx, reg_t ip, reg_t sp)
{
	memset(&actx->mctx, 0, sizeof(mcontext_t));
	memset((void *)actx->regs, 0, sizeof(reg_t) * ARCH_NREGS);
}

static inline int
arch_context_switch(arch_context_t *ca, arch_context_t *na)
{
	return 0;
}

#endif /* ARCH_AARCH64_CONTEXT_H */
