#ifndef ARCH_X86_64_CONTEXT_H
#define ARCH_X86_64_CONTEXT_H

#include <assert.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>
#include <stdbool.h>

#define ARCH_NREGS       (16 /* GP registers */ + 1 /* for IP */)
#define ARCH_SIG_JMP_OFF 8

/*
 * Register strict ordering>
 * rax = (regs + 0) = 0(%%reg)
 * rbx = (regs + 1) = 8(%%reg)
 * rcx = (regs + 2) = 16(%%reg)
 * rdx = (regs + 3) = 24(%%reg)
 * rbp = (regs + 4) = 32(%%reg)
 * rsp = (regs + 5) = 40(%%reg)
 * rsi = (regs + 6) = 48(%%reg)
 * rdi = (regs + 7) = 56(%%reg)
 * r8 = (regs + 8) = 64(%%reg)
 * r9 = (regs + 9) = 72(%%reg)
 * r10 = (regs + 10) = 80(%%reg)
 * r11 = (regs + 11) = 88(%%reg)
 * r12 = (regs + 12) = 96(%%reg)
 * r13 = (regs + 13) = 104(%%reg)
 * r14 = (regs + 14) = 112(%%reg)
 * r15 = (regs + 15) = 120(%%reg)
 * rip = (regs + 16) = 128(%%reg)
 */

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

extern void __attribute__((noreturn)) worker_thread_restore_preempted_sandbox(void);
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

	*(actx->regs + 5)  = sp;
	*(actx->regs + 16) = ip;
}

/**
 * Preempt the current sandbox and start executing the next sandbox
 * @param mc - the context of the current thread of execution
 * @param ctx - the context that we want to restore
 * @return {0,1} 0 = context restored successfully. 1 = special processing because thread was last in a user-level
 * context switch state
 **/
static int
arch_mcontext_restore(mcontext_t *mc, arch_context_t *ctx)
{
	assert(ctx != &worker_thread_base_context);

	// if ctx->regs[5] is set, this was last in a user-level context switch state
	bool did_user_level_context_switch = ctx->regs[5];
	if (did_user_level_context_switch) {
		mc->gregs[REG_RSP] = ctx->regs[5];
		mc->gregs[REG_RIP] = ctx->regs[16] + ARCH_SIG_JMP_OFF;
		ctx->regs[5]       = 0;
		return 1;
	} else {
		// Restore mcontext
		memcpy(mc, &ctx->mctx, sizeof(mcontext_t));
		memset(&ctx->mctx, 0, sizeof(mcontext_t));
		return 0;
	}
}

/**
 * Save the context of the currently executing process
 * @param ctx - destination
 * @param mc - source
 **/
static void
arch_mcontext_save(arch_context_t *ctx, const mcontext_t *mc)
{
	assert(ctx != &worker_thread_base_context);

	ctx->regs[5] = 0;
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
arch_context_switch(arch_context_t *current, arch_context_t *next)
{
	// if both current and next are NULL, there is no state change
	assert(current != NULL || next != NULL);
	assert(current != next);

	// Set any NULLs to worker_thread_base_context to resume execution of main
	if (current == NULL) current = &worker_thread_base_context;
	if (next == NULL) next = &worker_thread_base_context;

	reg_t *current_registers = current->regs, *next_registers = next->regs;
	// assert(current_registers && next_registers);

	// current_registers and next_registers are register sets representing the context we are switching from and to
	// They are laid out as follows:
	//  rax = (regs + 0) = 0(%%reg)
	//  rbx = (regs + 1) = 8(%%reg)
	//  rcx = (regs + 2) = 16(%%reg)
	//  rdx = (regs + 3) = 24(%%reg)
	//  rbp = (regs + 4) = 32(%%reg)
	//  rsp = (regs + 5) = 40(%%reg)
	//  rsi = (regs + 6) = 48(%%reg)
	//  rdi = (regs + 7) = 56(%%reg)
	//  r8 = (regs + 8) = 64(%%reg)
	//  r9 = (regs + 9) = 72(%%reg)
	//  r10 = (regs + 10) = 80(%%reg)
	//  r11 = (regs + 11) = 88(%%reg)
	//  r12 = (regs + 12) = 96(%%reg)
	//  r13 = (regs + 13) = 104(%%reg)
	//  r14 = (regs + 14) = 112(%%reg)
	//  r15 = (regs + 15) = 120(%%reg)
	//  rip = (regs + 16) = 128(%%reg)

	// TODO: What is the meaning of $2F? What is at that address? The worker_thread_base_context?
	// How do we know the address?

	asm volatile(
	  "pushq %%rbp\n\t"           // stack[stack_len++] = base_pointer
	  "movq %%rsp, %%rbp\n\t"     // base_pointer = stack_pointer. Start new Frame
	  "movq $2f, 128(%%rax)\n\t"  // current_registers[16] (instruction_pointer) = 2F (47).
	  "movq %%rsp, 40(%%rax)\n\t" // current_registers[5] (stack_pointer) = stack_pointer
	  "cmpq $0, 40(%%rbx)\n\t"    // if (next_registers[5] == 0)
	  "je 1f\n\t"                 // 	goto 1
	  "movq 40(%%rbx), %%rsp\n\t" // stack_pointer = next_registers[5] (stack_pointer)
	  "jmpq *128(%%rbx)\n\t"      // immediate jump to next_registers[16] (instruction_pointer)
	  "1:\n\t"                    // 1:
	  "call worker_thread_restore_preempted_sandbox\n\t" // 	worker_thread_restore_preempted_sandbox()
	  ".align 8\n\t"                                     //		V FALLTHROUGH V
	  "2:\n\t"                                           // 2:
	  "movq $0, 40(%%rbx)\n\t"                           //		next_registers[5] = 0;
	  ".align 8\n\t"                                     //		V FALLTHROUGH V
	  "3:\n\t"                                           // 3:
	  "popq %%rbp\n\t"                                   // 	base_pointer = stack[--stack_len]
	  :                                                  // Output List
	  : "a"(current_registers), "b"(next_registers)      // Input
	  : "memory", "cc", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "xmm0",
	    "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13",
	    "xmm14", "xmm15"); // Klobber List. This is long because of the context switch

	return 0;
}

#endif /* ARCH_X86_64_CONTEXT_H */
