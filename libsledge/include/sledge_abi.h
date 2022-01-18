#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <threads.h>

/* Do not include runtime headers here! */

/** ABI Types
 * Changes to these types breaks the contract between sledgert and the *.so modules that it runs. This means that all
 * modules must be recompiled. Avoid this!
 */

struct sledge_abi__wasm_table_entry {
	uint32_t type_id;
	void *   func_pointer;
};

struct sledge_abi__wasm_table {
	uint32_t                             length;
	uint32_t                             capacity;
	struct sledge_abi__wasm_table_entry *buffer; /* Backing heap allocation */
};

struct sledge_abi__wasm_memory {
	uint64_t size;     /* Initial Size in bytes */
	uint64_t capacity; /* Size backed by actual pages */
	uint64_t max;      /* Soft cap in bytes. Defaults to 4GB */
	uint8_t *buffer;   /* Backing heap allocation. Different lifetime because realloc might move this */
};

/* This structure is the runtime representation of the unique state of a module instance
 * Currently this is not spec-compliant, as it only supports a single table and a single memory and it excludes many
 * entities https://webassembly.github.io/spec/core/exec/runtime.html#module-instances
 */
struct sledge_abi__wasm_module_instance {
	struct sledge_abi__wasm_table *table;
	/* Note: memory has an opaque type due to private state. Do not reorder members below this! */
	struct sledge_abi__wasm_memory memory;
	/* Private runtime state follows */
};

/* Based on example traps listed at https://webassembly.org/docs/security/ */
enum sledge_abi__wasm_trap
{
	WASM_TRAP_EXIT                          = 1,
	WASM_TRAP_INVALID_INDEX                 = 2,
	WASM_TRAP_MISMATCHED_FUNCTION_TYPE      = 3,
	WASM_TRAP_PROTECTED_CALL_STACK_OVERFLOW = 4,
	WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY   = 5,
	WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION  = 6,
	WASM_TRAP_MISMATCHED_GLOBAL_TYPE        = 7,
	WASM_TRAP_COUNT
};

/* Symbols expected from sledgert */

extern void    sledge_abi__wasm_trap_raise(enum sledge_abi__wasm_trap trapno);
extern int32_t sledge_abi__wasm_memory_expand(struct sledge_abi__wasm_memory *wasm_memory, uint32_t page_count);
void           sledge_abi__wasm_memory_initialize_region(struct sledge_abi__wasm_memory *wasm_memory, uint32_t offset,
                                                         uint32_t region_size, uint8_t region[]);

extern int32_t sledge_abi__wasm_globals_get_i32(uint32_t idx);
extern int64_t sledge_abi__wasm_globals_get_i64(uint32_t idx);
extern int32_t sledge_abi__wasm_globals_set_i32(uint32_t idx, int32_t value, bool is_mutable);
extern int32_t sledge_abi__wasm_globals_set_i64(uint32_t idx, int64_t value, bool is_mutable);

/* Wasm initialization functions generated by the aWsm compiler */
extern void     sledge_abi__init_globals(void);
extern void     sledge_abi__init_mem(void);
extern void     sledge_abi__init_tbl(void);
extern int32_t  sledge_abi__entrypoint(void);
extern uint32_t sledge_abi__wasm_memory_starting_pages(void);
extern uint32_t sledge_abi__wasm_memory_max_pages(void);

typedef void (*sledge_abi__init_globals_fn_t)(void);
#define SLEDGE_ABI__INITIALIZE_GLOBALS "sledge_abi__init_globals"

typedef void (*sledge_abi__init_mem_fn_t)(void);
#define SLEDGE_ABI__INITIALIZE_MEMORY "sledge_abi__init_mem"

typedef void (*sledge_abi__init_tbl_fn_t)(void);
#define SLEDGE_ABI__INITIALIZE_TABLE "sledge_abi__init_tbl"

typedef int32_t (*sledge_abi__entrypoint_fn_t)(void);
#define SLEDGE_ABI__ENTRYPOINT "sledge_abi__entrypoint"

typedef uint32_t (*sledge_abi__wasm_memory_starting_pages_fn_t)(void);
#define SLEDGE_ABI__STARTING_PAGES "sledge_abi__wasm_memory_starting_pages"

typedef uint32_t (*sledge_abi__wasm_memory_max_pages_fn_t)(void);
#define SLEDGE_ABI__MAX_PAGES "sledge_abi__wasm_memory_max_pages"
