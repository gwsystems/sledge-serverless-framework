#pragma once

#include <stdint.h>
#include <stdio.h>


#ifndef PAGE_SIZE
#define PAGE_SIZE (1 << 12)
#endif

/* For this family of macros, do NOT pass zero as the pow2 */
#define round_to_pow2(x, pow2)    (((unsigned long)(x)) & (~((pow2)-1)))
#define round_up_to_pow2(x, pow2) (round_to_pow2(((unsigned long)x) + (pow2)-1, (pow2)))

#define round_to_page(x)    round_to_pow2(x, PAGE_SIZE)
#define round_up_to_page(x) round_up_to_pow2(x, PAGE_SIZE)

#define EXPORT       __attribute__((visibility("default")))
#define IMPORT       __attribute__((visibility("default")))
#define INLINE       __attribute__((always_inline))
#define PAGE_ALIGNED __attribute__((aligned(PAGE_SIZE)))
#define WEAK         __attribute__((weak))

/* FIXME: per-module configuration? Issue #101 */
#define WASM_PAGE_SIZE   (1024 * 64) /* 64KB */
#define WASM_START_PAGES (1 << 8)    /* 16MB */
#define WASM_MAX_PAGES   (1 << 15)   /* 4GB */
#define WASM_STACK_SIZE  (1 << 19)   /* 512KB */

/* These are per module symbols and I'd need to dlsym for each module. instead just use global constants, see above
macros. The code generator compiles in the starting number of wasm pages, and the maximum number of pages If we try
and allocate more than max_pages, we should fault */

/* The code generator also compiles in stubs that populate the linear memory and function table */
void populate_memory(void);
void populate_table(void);

/* memory also provides the table access functions */
#define INDIRECT_TABLE_SIZE (1 << 10)

struct indirect_table_entry {
	uint32_t type_id;
	void *   func_pointer;
};

/* Cache of Frequently Accessed Members used to avoid pointer chasing */
struct sandbox_context_cache {
	struct indirect_table_entry *module_indirect_table;
	void *                       linear_memory_start;
	uint32_t                     linear_memory_size;
};

extern __thread struct sandbox_context_cache local_sandbox_context_cache;

/* functions in the module to lookup and call per sandbox. */
typedef int32_t (*mod_main_fn_t)(int32_t a, int32_t b);
typedef void (*mod_glb_fn_t)(void);
typedef void (*mod_mem_fn_t)(void);
typedef void (*mod_tbl_fn_t)(void);
typedef void (*mod_libc_fn_t)(int32_t, int32_t);
