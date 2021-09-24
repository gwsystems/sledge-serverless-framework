#pragma once

#include <stdint.h>

/* FIXME: per-module configuration? Issue #101 */
#define WASM_PAGE_SIZE            (1024 * 64) /* 64KB */
#define WASM_MEMORY_PAGES_INITIAL (1 << 8)    /* 256 Pages ~16MB */
#define WASM_MEMORY_PAGES_MAX     (1 << 15)   /* 32,768 Pages ~4GB */

#define WASM_STACK_SIZE (1 << 19) /* 512KB */

/* bytes, not wasm pages */
struct wasm_memory {
	void *   start;
	uint32_t size;
	uint64_t max;
};

/* Based on example traps listed at https://webassembly.org/docs/security/ */
typedef enum
{
	WASM_TRAP_EXIT                          = 1,
	WASM_TRAP_INVALID_INDEX                 = 2,
	WASM_TRAP_MISMATCHED_FUNCTION_TYPE      = 3,
	WASM_TRAP_PROTECTED_CALL_STACK_OVERFLOW = 4,
	WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY   = 5,
	WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION  = 6,
	WASM_TRAP_COUNT
} wasm_trap_t;
