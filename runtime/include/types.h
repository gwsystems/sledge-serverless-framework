#pragma once

#include <stdint.h>
#include <stdio.h>
#include <threads.h>

#include "wasm_types.h"

/* For this family of macros, do NOT pass zero as the pow2 */
#define round_to_pow2(x, pow2)    (((unsigned long)(x)) & (~((pow2)-1)))
#define round_up_to_pow2(x, pow2) (round_to_pow2(((unsigned long)(x)) + (pow2)-1, (pow2)))

#define round_to_page(x)    round_to_pow2(x, PAGE_SIZE)
#define round_up_to_page(x) round_up_to_pow2(x, PAGE_SIZE)

#define EXPORT       __attribute__((visibility("default")))
#define IMPORT       __attribute__((visibility("default")))
#define INLINE       __attribute__((always_inline))
#define PAGE_ALIGNED __attribute__((aligned(PAGE_SIZE)))
#define PAGE_SIZE    (unsigned long)(1 << 12)
#define WEAK         __attribute__((weak))

/* memory also provides the table access functions */
#define INDIRECT_TABLE_SIZE (1 << 10)

struct indirect_table_entry {
	uint32_t type_id;
	void *   func_pointer;
};

/* Cache of Frequently Accessed Members used to avoid pointer chasing */
struct sandbox_context_cache {
	struct wasm_memory           memory;
	void *wasi_context;
	struct indirect_table_entry *module_indirect_table;
};

extern thread_local struct sandbox_context_cache local_sandbox_context_cache;
