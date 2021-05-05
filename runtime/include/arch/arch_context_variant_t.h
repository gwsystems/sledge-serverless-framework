#pragma once

#include "panic.h"

/* The enum is compared directly in assembly, so maintain integral values! */
typedef enum
{
	ARCH_CONTEXT_VARIANT_UNUSED  = 0, /* Has not have saved a context */
	ARCH_CONTEXT_VARIANT_FAST    = 1, /* Saved a fastpath context */
	ARCH_CONTEXT_VARIANT_SLOW    = 2, /* Saved a slowpath context */
	ARCH_CONTEXT_VARIANT_RUNNING = 3  /* Context is executing and content is out of date */
} arch_context_variant_t;

static inline char *
arch_context_variant_print(arch_context_variant_t context)
{
	switch (context) {
	case ARCH_CONTEXT_VARIANT_UNUSED:
		return "Unused";
	case ARCH_CONTEXT_VARIANT_FAST:
		return "Fast";
	case ARCH_CONTEXT_VARIANT_SLOW:
		return "Slow";
	case ARCH_CONTEXT_VARIANT_RUNNING:
		return "Running";
	default:
		panic("Encountered unexpected arch_context variant: %u\n", context);
	}
}
