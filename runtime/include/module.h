#pragma once

#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "admissions_control.h"
#include "admissions_info.h"
#include "awsm_abi.h"
#include "current_wasm_module_instance.h"
#include "http.h"
#include "panic.h"
#include "pool.h"
#include "types.h"
#include "wasm_stack.h"
#include "wasm_memory.h"
#include "wasm_table.h"

#define MODULE_DEFAULT_REQUEST_RESPONSE_SIZE (PAGE_SIZE)

#define MODULE_MAX_NAME_LENGTH 32
#define MODULE_MAX_PATH_LENGTH 256

extern thread_local int worker_thread_idx;

INIT_POOL(wasm_memory, wasm_memory_free)
INIT_POOL(wasm_stack, wasm_stack_free)

/*
 * Defines the listen backlog, the queue length for completely established socketeds waiting to be accepted
 * If this value is greater than the value in /proc/sys/net/core/somaxconn (typically 128), then it is silently
 * truncated to this value. See man listen(2) for info
 *
 * When configuring the number of sockets to handle, the queue length of incomplete sockets defined in
 * /proc/sys/net/ipv4/tcp_max_syn_backlog should also be considered. Optionally, enabling syncookies removes this
 * maximum logical length. See tcp(7) for more info.
 */
#define MODULE_MAX_PENDING_CLIENT_REQUESTS 128
#if MODULE_MAX_PENDING_CLIENT_REQUESTS > 128
#warning \
  "MODULE_MAX_PENDING_CLIENT_REQUESTS likely exceeds the value in /proc/sys/net/core/somaxconn and thus may be silently truncated";
#endif

/* TODO: Dynamically size based on number of threads */
#define MAX_WORKER_THREADS 64

struct module_pools {
	struct wasm_memory_pool memory;
	struct wasm_stack_pool  stack;
} __attribute__((aligned(CACHE_PAD)));

struct module {
	/* Metadata from JSON Config */
	char                   name[MODULE_MAX_NAME_LENGTH];
	char                   path[MODULE_MAX_PATH_LENGTH];
	uint32_t               stack_size; /* a specification? */
	uint32_t               relative_deadline_us;
	int                    port;
	struct admissions_info admissions_info;
	uint64_t               relative_deadline; /* cycles */

	/* HTTP State */
	size_t             max_request_size;
	size_t             max_response_size;
	char               response_content_type[HTTP_MAX_HEADER_VALUE_LENGTH];
	struct sockaddr_in socket_address;
	int                socket_descriptor;

	/* Handle and ABI Symbols for *.so file */
	struct awsm_abi abi;

	_Atomic uint32_t   reference_count; /* ref count how many instances exist here. */
	struct wasm_table *indirect_table;

	struct module_pools pools[MAX_WORKER_THREADS];
};

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
 * enabled. We are check that current_wasm_module_instance.table is NULL to gain confidence that
 * we are not invoking this in a way that clobbers a current module.
 *
 * If we want to be able to do this later, we can possibly defer module_initialize_table until the first
 * invocation. Alternatively, we can maintain the table per sandbox and call initialize
 * on each sandbox if this "assumption" is too restrictive and we're ready to pay a per-sandbox performance hit.
 */
static inline void
module_initialize_table(struct module *module)
{
	assert(current_wasm_module_instance.table == NULL);
	current_wasm_module_instance.table = module->indirect_table;
	module->abi.initialize_tables();
	current_wasm_module_instance.table = NULL;
}

static inline int
module_alloc_table(struct module *module)
{
	/* WebAssembly Indirect Table */
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
	for (int i = 0; i < MAX_WORKER_THREADS; i++) {
		wasm_memory_pool_init(&module->pools[i].memory, false);
		wasm_stack_pool_init(&module->pools[i].stack, false);
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
 * @param argc standard UNIX count of arguments
 * @param argv standard UNIX vector of arguments
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
	wasm_memory_reinit(memory, module->abi.starting_pages * WASM_PAGE_SIZE);
	wasm_memory_pool_add_nolock(&module->pools[worker_thread_idx].memory, memory);
}

/********************************
 * Public Methods from module.c *
 *******************************/

void           module_free(struct module *module);
struct module *module_alloc(char *mod_name, char *mod_path, uint32_t stack_sz, uint32_t relative_deadline_us, int port,
                            int req_sz, int resp_sz, int admissions_percentile, uint32_t expected_execution_us);
int            module_alloc_from_json(char *filename);
