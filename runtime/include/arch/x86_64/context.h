#pragma once

#include "arch/common.h"
#include "types.h"

#define ARCH_SIG_JMP_OFF 8

/**
 * Initializes a context, zeros out registers, and sets the Instruction and
 * Stack pointers
 * @param actx arch_context to init
 * @param ip value to set instruction pointer to
 * @param sp value to set stack pointer to
 */
static void __attribute__((noinline)) arch_context_init(struct arch_context *actx, reg_t ip, reg_t sp)
{
	memset(&actx->mctx, 0, sizeof(mcontext_t));
	memset((void *)actx->regs, 0, sizeof(reg_t) * UREG_COUNT);

	if (sp) {
		/*
		 * context_switch conventions: bp is expected to be on top of the stack
		 * when co-op context switching..
		 *
		 * Temporarily switches the active stack to the stack pointer stored in sp
		 * to push the stack pointer sp to the top of its own stack.
		 * This acts as the base pointer
		 */
		asm volatile("movq %%rsp, %%rbx\n\t" /* Temporarily save pointer of active stack to B */
		             "movq %%rax, %%rsp\n\t" /* Set active stack to stack pointer in A(C variable sp) */
		             "pushq %%rax\n\t"       /* Push A(C variable sp) onto the stack at sp */
		             "movq %%rsp, %%rax\n\t" /* Write the incremented stack pointer to A(C variable sp) */
		             "movq %%rbx, %%rsp\n\t" /* Restore original stack saved in B */
		             : "=a"(sp)
		             : "a"(sp)
		             : "memory", "cc", "rbx");
	}

	actx->regs[UREG_RSP] = sp;
	actx->regs[UREG_RIP] = ip;
	actx->variant        = ARCH_CONTEXT_QUICK;
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
	assert(software_interrupt_is_disabled);

	if (next->variant == ARCH_CONTEXT_QUICK && (next->regs[UREG_RIP] == 0 || next->regs[UREG_RSP] == 0)) {
		debuglog("Next Context was Quick Variant, but data was invalid.");
		assert(0);
	}

	/* if both current and next are NULL, there is no state change */
	assert(current != NULL || next != NULL);

	/* Assumption: The caller does not switch to itself */
	assert(current != next);

	/* Set any NULLs to worker_thread_base_context to resume execution of main */
	if (current == NULL) current = &worker_thread_base_context;
	if (next == NULL) next = &worker_thread_base_context;

	/* Assumption: The context we are switching to should have saved a context in some form */
	assert(next->variant == ARCH_CONTEXT_QUICK || next->variant != ARCH_CONTEXT_UNUSED);

	reg_t *current_registers = current->regs, *next_registers = next->regs;
	assert(current_registers && next_registers);

	asm volatile(
	  /* Create a new stack frame */
	  "pushq %%rbp\n\t"       /*  stack[stack_len++] = base_pointer */
	  "movq %%rsp, %%rbp\n\t" /*  base_pointer = stack_pointer. Start new Frame */

	  /*
	   * Save the IP and stack pointer to the context of the sandbox we're switching from
	   */
	  "movq $2f, 8(%%rax)\n\t"  /* Write the address of label 2 to current_registers[1] (instruction_pointer). */
	  "movq %%rsp, (%%rax)\n\t" /* current_registers[0] (stack_pointer) = stack_pointer */
	  "movq $1, (%%rcx)\n\t"    /* current->variant = ARCH_CONTEXT_QUICK; */

	  /*
	   * Check if the variant of the context we're trying to switch to is SLOW (mcontext-based)
	   * If it is, jump to label 1 to restore the preempted sandbox
	   */
	  "cmpq $2, (%%rcx)\n\t" /* if (next->variant == ARCH_CONTEXT_SLOW); */
	  "je 1f\n\t"            /* 	goto 1; restore the existing sandbox using mcontext */

	  /*
	   * Fast Path
	   * We can just write update the stack pointer and jump to the target instruction
	   */
	  "movq (%%rbx), %%rsp\n\t" /* stack_pointer = next_registers[0] (stack_pointer) */
	  "jmpq *8(%%rbx)\n\t"      /* immediate jump to next_registers[1] (instruction_pointer) */

	  /*
	   * Slow Path
	   * If the variant is ARCH_CONTEXT_SLOW, that means the sandbox was preempted and we need to
	   * fallback to a full mcontext-based context switch. We do this by invoking
	   * arch_context_mcontext_restore,  which fires a SIGUSR1 signal. The SIGUSR1 signal handler
	   * executes the mcontext-based context switch.
	   */
	  "1:\n\t"
	  "call arch_context_mcontext_restore\n\t"
	  ".align 8\n\t"

	  /*
	   * This label was written to the instruction pointer of the sandbox that was switched away from
	   * The sandbox either resumes at label 2 or 3 depending on if an offset of 8 is used.
	   */
	  "2:\n\t"
	  "movq $3, (%%rdx)\n\t" /* next->variant = ARCH_CONTEXT_QUICK; */
	  ".align 8\n\t"

	  /* This label is used in conjunction with a static offset */
	  "3:\n\t"
	  /* TODO: Should we set  next->variant = ARCH_CONTEXT_SLOW here?;*/
	  "popq %%rbp\n\t" /* base_pointer = stack[--stack_len]; Base Pointer is restored */
	  :
	  : "a"(current_registers), "b"(next_registers), "c"(&current->variant), "d"(&next->variant)
	  : "memory", "cc", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "xmm0", "xmm1", "xmm2",
	    "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14",
	    "xmm15");

	worker_thread_is_switching_context = false;
	return 0;
}
