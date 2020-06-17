#ifndef ARCH_X86_64_CONTEXT_H
#define ARCH_X86_64_CONTEXT_H

#include <assert.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>
#include <stdbool.h>

#define ARCH_SIG_JMP_OFF (8)

// Userspace Registers.
enum UREGS
{
	UREG_RSP,
	UREG_RIP,
	UREG_COUNT
};

typedef uint64_t reg_t;

/*
 * This is the slowpath switch to a preempted sandbox!
 * SIGUSR1 on the current thread and restore mcontext there!
 */

struct arch_context {
	reg_t      regs[UREG_COUNT]; // Used during blocking.2: stack pointer and instruction pointer
	mcontext_t mctx;             // Used during preemption
};

extern void __attribute__((noreturn)) worker_thread_restore_preempted_sandbox(void);
extern __thread struct arch_context worker_thread_base_context;

static void __attribute__((noinline)) arch_context_init(struct arch_context *actx, reg_t ip, reg_t sp)
{
	memset(&actx->mctx, 0, sizeof(mcontext_t));
	memset((void *)actx->regs, 0, sizeof(reg_t) * UREG_COUNT);

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

	actx->regs[UREG_RSP] = sp;
	actx->regs[UREG_RIP] = ip;
}

/**
 * Preempt the current sandbox and start executing the next sandbox
 * @param mc - the context of the current thread of execution
 * @param ctx - the context that we want to restore
 * context switch state
 **/
static void
arch_mcontext_restore(mcontext_t *mc, struct arch_context *ctx)
{
	assert(ctx != &worker_thread_base_context);
	assert(ctx->regs[UREG_RSP] == 0);
	memcpy(mc, &ctx->mctx, sizeof(mcontext_t));
	memset(&ctx->mctx, 0, sizeof(mcontext_t));
}

static void
arch_context_restore(mcontext_t *mc, struct arch_context *ctx)
{
	assert(ctx != &worker_thread_base_context);
	assert(ctx->regs[UREG_RSP]);
	mc->gregs[REG_RSP]  = ctx->regs[UREG_RSP];
	mc->gregs[REG_RIP]  = ctx->regs[UREG_RIP] + ARCH_SIG_JMP_OFF;
	ctx->regs[UREG_RSP] = 0;
	ctx->regs[UREG_RIP] = 0;
}

/**
 * Save the context of the currently executing process
 * @param ctx - destination
 * @param mc - source
 **/
static void
arch_mcontext_save(struct arch_context *ctx, const mcontext_t *mc)
{
	assert(ctx != &worker_thread_base_context);
	ctx->regs[UREG_RSP] = 0;
	memcpy(&ctx->mctx, mc, sizeof(mcontext_t));
}

/**
 * @param current - the registers and context of the thing running
 * @param next - the registers and context of what we're switching to
 * @return always returns 0, indicating success
 *
 * NULL in either of these values indicates the "no sandbox to execute" state,
 * which defaults to resuming execution of main
 **/
static inline int
arch_context_switch(struct arch_context *current, struct arch_context *next)
{
	// if both current and next are NULL, there is no state change
	assert(current != NULL || next != NULL);
	assert(current != next);

	// Set any NULLs to worker_thread_base_context to resume execution of main
	if (current == NULL) current = &worker_thread_base_context;
	if (next == NULL) next = &worker_thread_base_context;

	reg_t *current_registers = current->regs, *next_registers = next->regs;
	assert(current_registers && next_registers);

	asm volatile(
	  // Create a new stack frame
	  "pushq %%rbp\n\t"       // stack[stack_len++] = base_pointer
	  "movq %%rsp, %%rbp\n\t" // base_pointer = stack_pointer. Start new Frame

	  // Save the IP and stack pointer to the context of the sandbox we're switching from
	  "movq $2f, 8(%%rax)\n\t"  // Write the address of label 2 to current_registers[1] (instruction_pointer).
	  "movq %%rsp, (%%rax)\n\t" // current_registers[0] (stack_pointer) = stack_pointer

	  // Check if the stack pointer of the context we're trying to switch to is 0.
	  // If the stack pointer is 0, we were preempted, so we have to restore using mcontext
	  // If it is, the context cannot be loaded, so jump to label 0 to restore the preempted sandbox
	  "cmpq $0, (%%rbx)\n\t" // if (next_registers[0] == 0); If the stack pointer destination context is 0,
	                         // restore the existing sandbox
	  "je 1f\n\t"            // 	goto 1

	  // Success Case
	  // If the stack pointer we're trying to switch to is not 0, write it to the actual stack pointer
	  // and jump to the target instruction
	  "movq (%%rbx), %%rsp\n\t" // stack_pointer = next_registers[5] (stack_pointer)
	  "jmpq *8(%%rbx)\n\t"      // immediate jump to next_registers[16] (instruction_pointer)

	  // Failure Case
	  // The state of the context we're trying to switch to is invalid, so restore the sandbox
	  // 1. By executing worker_thread_restore_preempted_sandbox to fires off a SIGUSR1, triggering a signal
	  // handler,
	  // 2. By writing 0 to 40(%%rbx). I would have expected this to be an invarient because we
	  // used cmpq $0, 40(%%rbx) as our branching logic. Can we not assume this because of the signal handler?
	  // 3. By restoring the existing stack frame
	  "1:\n\t"                                           // 1:
	  "call worker_thread_restore_preempted_sandbox\n\t" // 	worker_thread_restore_preempted_sandbox()
	  ".align 8\n\t"                                     //		V FALLTHROUGH V
	  "2:\n\t"                                           // 2:
	  "movq $0, (%%rbx)\n\t"                             //		next_registers[5] = 0;
	  ".align 8\n\t"                                     //		V FALLTHROUGH V
	  "3:\n\t"                                           // 3:
	  "popq %%rbp\n\t"                                   // 	base_pointer = stack[--stack_len]

	  // Output List
	  :
	  // Input List
	  : "a"(current_registers), "b"(next_registers)
	  // Clobber List. This is long because of the context switch
	  : "memory", "cc", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "xmm0",
	    "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13",
	    "xmm14", "xmm15");

	return 0;
}

#endif /* ARCH_X86_64_CONTEXT_H */
