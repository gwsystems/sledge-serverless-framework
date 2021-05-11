#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "arch/context.h"
#include "current_sandbox.h"
#include "sandbox_types.h"
#include "sandbox_functions.h"
#include "software_interrupt.h"

/**
 * @brief Switches to the base context, placing the current sandbox on the completion queue if in RETURNED state
 */
static inline void
current_sandbox_yield()
{
	assert(!software_interrupt_is_enabled());

	struct sandbox *current_sandbox = current_sandbox_get();
#ifndef NDEBUG
	if (current_sandbox != NULL) {
		assert(current_sandbox->state < SANDBOX_STATE_COUNT);
		assert(current_sandbox->stack_size == current_sandbox->module->stack_size);
	}
#endif

	/* Assumption: Base Context should never switch to Base Context */
	assert(current_sandbox != NULL);
	struct arch_context *current_context = &current_sandbox->ctxt;
	assert(current_context != &worker_thread_base_context);

#ifdef LOG_CONTEXT_SWITCHES
	debuglog("Sandbox %lu (@%p) (%s) > Base Context (@%p) (%s)\n", current_sandbox->id, current_context,
	         arch_context_variant_print(current_sandbox->ctxt.variant), &worker_thread_base_context,
	         arch_context_variant_print(worker_thread_base_context.variant));
#endif

	sandbox_exit(current_sandbox);
	current_sandbox_set(NULL);
	assert(worker_thread_base_context.variant == ARCH_CONTEXT_VARIANT_FAST);
	runtime_worker_threads_deadline[worker_thread_idx] = UINT64_MAX;
	arch_context_switch(current_context, &worker_thread_base_context);
	software_interrupt_enable();
}
