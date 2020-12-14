#pragma once

#include <assert.h>
#include "arch/common.h"
#include "current_sandbox.h"

#define ARCH_SIG_JMP_OFF 0x100 /* Based on code generated! */

/**
 * Initializes a context, zeros out registers, and sets the Instruction and
 * Stack pointers. Sets variant to unused if ip and sp are 0, fast otherwise.
 * @param actx arch_context to init
 * @param ip value to set instruction pointer to
 * @param sp value to set stack pointer to
 */

static inline void
arch_context_init(struct arch_context *actx, reg_t ip, reg_t sp)
{
	assert(actx != NULL);
	assert(!software_interrupt_is_enabled());

	if (ip == 0 && sp == 0) {
		actx->variant = ARCH_CONTEXT_VARIANT_UNUSED;
	} else {
		actx->variant = ARCH_CONTEXT_VARIANT_FAST;
	}

	actx->regs[UREG_SP] = sp;
	actx->regs[UREG_IP] = ip;
}

/**
 * Restore a sandbox saved using a fastpath switch, restoring only the
 * instruction pointer and stack pointer registers rather than
 * a full mcontext, so it is less expensive than arch_mcontext_restore.
 * @param active_context - the context of the current worker thread
 * @param sandbox_context - the context that we want to restore
 */
static void
arch_context_restore(mcontext_t *active_context, struct arch_context *sandbox_context)
{
	assert(active_context != NULL);
	assert(sandbox_context != NULL);

	/* Assumption: Base Context is only ever used by arch_context_switch */
	assert(sandbox_context != &worker_thread_base_context);

	assert(sandbox_context->regs[UREG_SP]);
	assert(sandbox_context->regs[UREG_IP]);

	/* Transitioning from Fast -> Running */
	assert(sandbox_context->variant == ARCH_CONTEXT_VARIANT_FAST);
	sandbox_context->variant = ARCH_CONTEXT_VARIANT_RUNNING;

	active_context->sp = sandbox_context->regs[UREG_SP];
	active_context->pc = sandbox_context->regs[UREG_IP] + ARCH_SIG_JMP_OFF;
}

/**
 * @param a - the registers and context of the thing running
 * @param b - the registers and context of what we're switching to
 * @return always returns 0, indicating success
 *
 * NULL in either of these values indicates the "no sandbox to execute" state,
 * which defaults to resuming execution of main
 */
static inline int
arch_context_switch(struct arch_context *a, struct arch_context *b)
{
	/* Assumption: Software Interrupts are disabled by caller */
	assert(!software_interrupt_is_enabled());

#ifndef NDEBUG
	/*
	 * Assumption: In the case of a slow context switch, the caller
	 * set current_sandbox to the sandbox containing the target context
	 */
	if (b->variant == ARCH_CONTEXT_VARIANT_SLOW) {
		struct sandbox *current = current_sandbox_get();
		assert(current != NULL && b == &current->ctxt);
	}
#endif

	/* if both a and b are NULL, there is no state change */
	assert(a != NULL || b != NULL);

	/* Assumption: The caller does not switch to itself */
	assert(a != b);

	/* Set any NULLs to worker_thread_base_context to resume execution of main */
	if (a == NULL) a = &worker_thread_base_context;
	if (b == NULL) b = &worker_thread_base_context;

	/* A Transition {Unused, Running} -> Fast */
	assert(a->variant == ARCH_CONTEXT_VARIANT_UNUSED || a->variant == ARCH_CONTEXT_VARIANT_RUNNING);

	/* B Transition {Fast, Slow} -> Running */
	assert(b->variant == ARCH_CONTEXT_VARIANT_FAST || b->variant == ARCH_CONTEXT_VARIANT_SLOW);

	/* Assumption: Fastpath state is well formed */
	if (b->variant == ARCH_CONTEXT_VARIANT_FAST) {
		assert(b->regs[UREG_IP] != 0);
		assert(b->regs[UREG_SP] != 0);
	}

	reg_t *a_registers = a->regs, *b_registers = b->regs;
	assert(a_registers && b_registers);

	asm volatile("mov x0, sp\n\t"
	             "adr x1, reset%=\n\t"
	             "str x1, [%[a], 8]\n\t"
	             "str x0, [%[a]]\n\t"
	             "mov x0, #1\n\t"
	             "str x0, [%[av]]\n\t"
	             "ldr x1, [%[bv]]\n\t"
	             "sub x1, x1, #2\n\t"
	             "cbz x1, slow%=\n\t"
	             "ldr x0, [%[b]]\n\t"
	             "ldr x1, [%[b], 8]\n\t"
	             "mov sp, x0\n\t"
	             "br x1\n\t"
	             "slow%=:\n\t"
	             "br %[slowpath]\n\t"
	             ".align 8\n\t"
	             "reset%=:\n\t"
	             "mov x1, #3\n\t"
	             "str x1, [%[bv]]\n\t"
	             ".align 8\n\t"
	             "exit%=:\n\t"
	             :
	             : [ a ] "r"(a_registers), [ b ] "r"(b_registers), [ av ] "r"(&a->variant), [ bv ] "r"(&b->variant),
	               [ slowpath ] "r"(&arch_context_restore_preempted)
	             : "memory", "cc", "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12",
	               "x13", "x14", "x15", "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23", "x24", "d8", "d9",
	               "d10", "d11", "d12", "d13", "d14", "d15");

	return 0;
}
