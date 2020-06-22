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


enum ARCH_CONTEXT
{
	ARCH_CONTEXT_UNUSED,
	ARCH_CONTEXT_QUICK,
	ARCH_CONTEXT_SLOW
};

struct arch_context {
	enum ARCH_CONTEXT variant;
	reg_t             regs[UREG_COUNT]; // Used during blocking.2: stack pointer and instruction pointer
	mcontext_t        mctx;             // Used during preemption
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
	actx->variant        = ARCH_CONTEXT_QUICK;
}

/**
 * Set the active context of the current worker thread to the provided sandbox.
 * This method restores the full mcontext stored in the arch_context struct, so while similar to arch_context_restore,
 * it is more expensive.
 * @param worker_thread_active_context - the context of the current worker thread
 * @param sandbox_context - the context that we want to restore
 **/
static void
arch_mcontext_restore(mcontext_t *worker_thread_active_context, struct arch_context *sandbox_context)
{
	assert(sandbox_context->variant == ARCH_CONTEXT_SLOW);
	assert(sandbox_context != NULL);
	assert(sandbox_context != &worker_thread_base_context);

	// We are setting the worker thread's active context to the mcontext stored in sandbox_context
	// So we assert that the instruction pointer and stack pointer in this context is not 0.
	if (sandbox_context->mctx.gregs[REG_RIP] == 0) {
		printf("Mcontext not set. Was quick context set? %lu\n", sandbox_context->regs[UREG_RIP]);
		assert(0);
	}
	// assert(sandbox_context->mctx.gregs[REG_RSP] != 0);
	if (sandbox_context->mctx.gregs[REG_RSP] == 0) {
		printf("Mcontext not set. Was quick context set? %lu\n", sandbox_context->regs[UREG_RSP]);
		assert(0);
	}

	memcpy(worker_thread_active_context, &sandbox_context->mctx, sizeof(mcontext_t));
	memset(&sandbox_context->mctx, 0, sizeof(mcontext_t));
}

/**
 * Restore a sandbox that was previously executing and preempted for higher-priority work.
 * This method restores only the instruction pointer and stack pointer registers rather than a full mcontext, so it is
 * less expensive than arch_mcontext_restore.
 * @param worker_thread_active_context - the context of the current worker thread
 * @param sandbox_context - the context that we want to restore
 **/
static void
arch_context_restore(mcontext_t *worker_thread_active_context, struct arch_context *sandbox_context)
{
	assert(sandbox_context->variant == ARCH_CONTEXT_QUICK);
	assert(sandbox_context != NULL);
	assert(sandbox_context != &worker_thread_base_context);

	// TODO: Phani explained that we need to be able to restore a sandbox with an IP of 0. Why is this?
	assert(sandbox_context->regs[UREG_RSP]);

	worker_thread_active_context->gregs[REG_RSP] = sandbox_context->regs[UREG_RSP];
	worker_thread_active_context->gregs[REG_RIP] = sandbox_context->regs[UREG_RIP] + ARCH_SIG_JMP_OFF;
	sandbox_context->regs[UREG_RSP]              = 0;
	sandbox_context->regs[UREG_RIP]              = 0;
}

/**
 * Save the full mcontext of the currently executing process
 * @param sandbox_context - destination
 * @param worker_thread_active_context - source
 **/
static void
arch_mcontext_save(struct arch_context *sandbox_context, const mcontext_t *worker_thread_active_context)
{
	assert(worker_thread_active_context != 0);
	assert(sandbox_context != &worker_thread_base_context);
	assert(worker_thread_active_context->gregs[REG_RIP] != 0);
	assert(worker_thread_active_context->gregs[REG_RSP] != 0);

	sandbox_context->regs[UREG_RSP] = 0;
	sandbox_context->regs[UREG_RIP] = 0;

	sandbox_context->variant = ARCH_CONTEXT_SLOW;
	memcpy(&sandbox_context->mctx, worker_thread_active_context, sizeof(mcontext_t));
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

	assert(next->variant == ARCH_CONTEXT_QUICK);


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
	  "movq (%%rbx), %%rsp\n\t" // stack_pointer = next_registers[0] (stack_pointer)
	  "jmpq *8(%%rbx)\n\t"      // immediate jump to next_registers[1] (instruction_pointer)

	  // Failure Case
	  // The state of the context we're trying to switch to is invalid, so restore the sandbox by executing
	  // worker_thread_restore_preempted_sandbox to fires off a SIGUSR1, triggering a signal handler,
	  "1:\n\t"                                           // 1:
	  "call worker_thread_restore_preempted_sandbox\n\t" // 	worker_thread_restore_preempted_sandbox()
	  ".align 8\n\t"                                     //		V FALLTHROUGH V

	  // Where the "switch from" sandbox resumes execution
	  "2:\n\t"               // 2:
	  "movq $0, (%%rbx)\n\t" //		next_registers[0] = 0; Stack Pointer is zeroed out
	  ".align 8\n\t"         //		V FALLTHROUGH V
	  "3:\n\t"               // 3:
	  "popq %%rbp\n\t"       // 	base_pointer = stack[--stack_len]; Base Pointer is restored

	  // Output List
	  :
	  // Input List
	  : "a"(current_registers), "b"(next_registers)
	  // Clobber List. This is long because of the context switch
	  : "memory", "cc", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "xmm0",
	    "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13",
	    "xmm14", "xmm15");

	next->variant = ARCH_CONTEXT_QUICK;

	return 0;
}

#endif /* ARCH_X86_64_CONTEXT_H */
