#pragma once

/*
 * This header is the single entry point into the arch_context code.
 * It includes processor independent code and conditionally includes architecture
 * dependent code.
 */

/*
 * These preprocessor conditionals load architecture-dependent code.
 *
 * Each of these headers include common.h, a sibling header that contains common
 * structures, enums, and externs used by the processor independent code. Those
 * definitions are thus transitively loaded here with the appropriate architecture
 * specific code.
 *
 * While the code in common.h could be located in this file above the conditionals,
 * structuring the code as such prevents IDEs from knowing how to resolve the
 * symbols when aarch64/context.h or x86_64/context is the active file.
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
 * Restore a full mcontext
 * Writes sandbox_context to active_context
 * active_context was saved to the stack by a signal handler
 * @param active_context - the context of the current worker thread
 * @param sandbox_context - the context that we want to restore
 */
static inline void
arch_mcontext_restore(mcontext_t *active_context, struct arch_context *sandbox_context)
{
	assert(!software_interrupt_is_enabled());

	assert(active_context != NULL);
	assert(sandbox_context != NULL);

	/* Assumption: Base Context is only ever used by arch_context_switch */
	assert(sandbox_context != &worker_thread_base_context);

	/* Transitioning from Slow -> Running */
	assert(sandbox_context->variant == ARCH_CONTEXT_VARIANT_SLOW);
	sandbox_context->variant = ARCH_CONTEXT_VARIANT_RUNNING;

	/* Restore mcontext */
	memcpy(active_context, &sandbox_context->mctx, sizeof(mcontext_t));
}


/**
 * Save the full mcontext of the currently executing process
 * @param sandbox_context - destination
 * @param active_context - source
 */
static inline void
arch_mcontext_save(struct arch_context *sandbox_context, const mcontext_t *active_context)
{
	/* Assumption: Only called indirectly via signal handler, so interrupts should be disabled */
	assert(!software_interrupt_is_enabled());

	assert(sandbox_context != NULL);
	assert(active_context != NULL);

	/* Assumption: The base context should never be modified */
	assert(sandbox_context != &worker_thread_base_context);

	/* Transitioning from {Unused, Running} -> Slow */
	assert(sandbox_context->variant == ARCH_CONTEXT_VARIANT_UNUSED
	       || sandbox_context->variant == ARCH_CONTEXT_VARIANT_RUNNING);
	sandbox_context->variant = ARCH_CONTEXT_VARIANT_SLOW;

	/* Copy mcontext */
	memcpy(&sandbox_context->mctx, active_context, sizeof(mcontext_t));
}
