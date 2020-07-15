#pragma once

#include "arch/common.h"

#define ARCH_SIG_JMP_OFF 0x100 /* Based on code generated! */

/**
 * Initializes a context, zeros out registers, and sets the Instruction and
 * Stack pointers
 * @param actx arch_context to init
 * @param ip value to set instruction pointer to
 * @param sp value to set stack pointer to
 */

static inline void
arch_context_init(struct arch_context *actx, reg_t ip, reg_t sp)
{
	memset(&actx->mctx, 0, sizeof(mcontext_t));
	memset((void *)actx->regs, 0, sizeof(reg_t) * UREG_COUNT);

	actx->regs[UREG_RSP] = sp;
	actx->regs[UREG_RIP] = ip;
}

/**
 * @param current - the registers and context of the thing running
 * @param next - the registers and context of what we're switching to
 * @return always returns 0, indicating success
 *
 * NULL in either of these values indicates the "no sandbox to execute" state,
 * which defaults to resuming execution of main
 */
static inline int
arch_context_switch(struct arch_context *current, struct arch_context *next)
{
	/* Assumption: Software Interrupts are disabled by caller */
	assert(!software_interrupt_is_enabled());

	/* if both current and next are NULL, there is no state change */
	assert(current != NULL || next != NULL);

	/* Assumption: The caller does not switch to itself */
	assert(current != next);

	/* Set any NULLs to worker_thread_base_context to resume execution of main */
	if (current == NULL) current = &worker_thread_base_context;
	if (next == NULL) next = &worker_thread_base_context;

	reg_t *current_registers = current->regs, *next_registers = next->regs;
	assert(current_registers && next_registers);

	asm volatile("mov x0, sp\n\t"
	             "adr x1, reset%=\n\t"
	             "str x1, [%[current], 8]\n\t"
	             "str x0, [%[current]]\n\t"
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
	             : [ current ] "r"(current_registers), [ next ] "r"(next_registers),
	               [ slowpath ] "r"(&arch_context_mcontext_restore)
	             : "memory", "cc", "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12",
	               "x13", "x14", "x15", "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25", "x26",
	               "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15");

	return 0;
}
