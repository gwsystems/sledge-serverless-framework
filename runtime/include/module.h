#pragma once

#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "admissions_control.h"
#include "admissions_info.h"
#include "current_wasm_module_instance.h"
#include "debuglog.h"
#include "panic.h"
#include "pool.h"
#include "sledge_abi_symbols.h"
#include "tcp_server.h"
#include "types.h"
#include "sledge_abi_symbols.h"
#include "wasm_stack.h"
#include "wasm_memory.h"
#include "wasm_table.h"

extern thread_local int worker_thread_idx;

INIT_POOL(wasm_memory, wasm_memory_free)
INIT_POOL(wasm_stack, wasm_stack_free)

struct module_pool {
	struct wasm_memory_pool memory;
	struct wasm_stack_pool  stack;
} CACHE_PAD_ALIGNED;

struct module {
	char    *path;
	uint32_t stack_size; /* a specification? */

	/* Handle and ABI Symbols for *.so file */
	struct sledge_abi_symbols abi;

	_Atomic uint32_t               reference_count; /* ref count how many instances exist here. */
	struct sledge_abi__wasm_table *indirect_table;

	struct module_pool *pools;
} CACHE_PAD_ALIGNED;

/********************************
 * Public Methods from module.c *
 *******************************/

void           module_free(struct module *module);
struct module *module_alloc(char *path);

/*************************
 * Public Static Inlines *
 ************************/

/**
 * Increment a modules reference count
 * @param module
 */
static inline void
module_acquire(struct module *module)
{
	assert(module->reference_count < UINT32_MAX);
	atomic_fetch_add(&module->reference_count, 1);
	return;
}

/**
 * Invoke a module's initialize_globals if the symbol was present in the *.so file.
 * This is present when aWsm is run with the --runtime-globals flag and absent otherwise.
 * @param module
 */
static inline void
module_initialize_globals(struct module *module)
{
	if (module->abi.initialize_globals != NULL) module->abi.initialize_globals();
}

/**
 * @brief Invoke a module's initialize_tables
 * @param module
 *
 * Table initialization calls a function that runs within the sandbox. Rather than setting the current sandbox,
 * we partially fake this out by only setting the table and then clearing after table
 * initialization is complete.
 *
 * assumption: This approach depends on module_alloc only being invoked at program start before preemption is
 * enabled. We are check that sledge_abi__current_wasm_module_instance.abi.table is NULL to gain confidence
 * that we are not invoking this in a way that clobbers a current module.
 *
 * If we want to be able to do this later, we can possibly defer module_initialize_table until the first
 * invocation. Alternatively, we can maintain the table per sandbox and call initialize
 * on each sandbox if this "assumption" is too restrictive and we're ready to pay a per-sandbox performance hit.
 */
static inline void
module_initialize_table(struct module *module)
{
	assert(sledge_abi__current_wasm_module_instance.abi.table == NULL);
	sledge_abi__current_wasm_module_instance.abi.table = module->indirect_table;
	module->abi.initialize_tables();
	sledge_abi__current_wasm_module_instance.abi.table = NULL;
}

static inline int
module_alloc_table(struct module *module)
{
	/* TODO: Should this be part of the module or per-sandbox? */
	/* TODO: How should this table be sized? */
	module->indirect_table = wasm_table_alloc(INDIRECT_TABLE_SIZE);
	if (module->indirect_table == NULL) return -1;

	module_initialize_table(module);
	return 0;
}

static inline void
module_initialize_pools(struct module *module)
{
	for (int i = 0; i < runtime_worker_threads_count; i++) {
		wasm_memory_pool_init(&module->pools[i].memory, false);
		wasm_stack_pool_init(&module->pools[i].stack, false);
	}
}

static inline void
module_deinitialize_pools(struct module *module)
{
	for (int i = 0; i < runtime_worker_threads_count; i++) {
		wasm_memory_pool_deinit(&module->pools[i].memory);
		wasm_stack_pool_deinit(&module->pools[i].stack);
	}
}

/**
 * Invoke a module's initialize_memory
 * @param module - the module whose memory we are initializing
 */
static inline void
module_initialize_memory(struct module *module)
{
	module->abi.initialize_memory();
}

/**
 * Invoke a module's entry function, forwarding on argc and argv
 * @param module
 * @return return code of module's main function
 */
static inline int32_t
module_entrypoint(struct module *module)
{
	return module->abi.entrypoint();
}

/**
 * Decrement a modules reference count
 * @param module
 */
static inline void
module_release(struct module *module)
{
	assert(module->reference_count > 0);
	atomic_fetch_sub(&module->reference_count, 1);
	return;
}

static inline struct wasm_stack *
module_allocate_stack(struct module *module)
{
	assert(module != NULL);

	struct wasm_stack *stack = wasm_stack_pool_remove_nolock(&module->pools[worker_thread_idx].stack);

	if (stack == NULL) {
		stack = wasm_stack_alloc(module->stack_size);
		if (unlikely(stack == NULL)) return NULL;
	}

	return stack;
}

static inline void
module_free_stack(struct module *module, struct wasm_stack *stack)
{
	wasm_stack_reinit(stack);
	wasm_stack_pool_add_nolock(&module->pools[worker_thread_idx].stack, stack);
}

static inline struct wasm_memory *
module_allocate_linear_memory(struct module *module)
{
	assert(module != NULL);

	uint64_t starting_bytes = (uint64_t)module->abi.starting_pages * WASM_PAGE_SIZE;
	uint64_t max_bytes      = (uint64_t)module->abi.max_pages * WASM_PAGE_SIZE;

	/* UINT32_MAX is the largest representable integral value that can fit into type uint32_t. Because C counts from
	zero, the number of states in the range 0..UINT32_MAX is thus UINT32_MAX + 1. This means that the maximum
	possible buffer that can be byte-addressed by a full 32-bit address space is UNIT32_MAX + 1 */
	assert(starting_bytes <= (uint64_t)UINT32_MAX + 1);
	assert(max_bytes <= (uint64_t)UINT32_MAX + 1);

	struct wasm_memory *linear_memory = wasm_memory_pool_remove_nolock(&module->pools[worker_thread_idx].memory);
	if (linear_memory == NULL) {
		linear_memory = wasm_memory_alloc(starting_bytes, max_bytes);
		if (unlikely(linear_memory == NULL)) return NULL;
	}

	return linear_memory;
}

static inline void
module_free_linear_memory(struct module *module, struct wasm_memory *memory)
{
    debuglog("Sandbox freeing %d memory\n", (int32_t)memory->size);
	wasm_memory_reinit(memory, module->abi.starting_pages * WASM_PAGE_SIZE);
	wasm_memory_pool_add_nolock(&module->pools[worker_thread_idx].memory, memory);
}
