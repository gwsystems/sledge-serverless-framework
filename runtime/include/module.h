#pragma once

#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "admissions_control.h"
#include "admissions_info.h"
#include "awsm_abi.h"
#include "http.h"
#include "panic.h"
#include "types.h"

#define MODULE_DEFAULT_REQUEST_RESPONSE_SIZE (PAGE_SIZE)

#define MODULE_MAX_NAME_LENGTH 32
#define MODULE_MAX_PATH_LENGTH 256

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

enum MULTI_TENANCY_CLASS
{
	MT_DEFAULT,
	MT_GUARANTEED
};

struct perworker_module_sandbox_queue {
	struct priority_queue *  sandboxes;
	struct module *          module;   // to be able to find the RB/MB/RP/RT.
	enum MULTI_TENANCY_CLASS mt_class; // check whether the corresponding PWM has been demoted
} __attribute__((aligned(128)));

struct module_global_request_queue {
	struct priority_queue *  sandbox_requests;
	struct module *          module;
	enum MULTI_TENANCY_CLASS mt_class;
};

struct module_timeout {
	uint64_t       timeout;
	struct module *module;
};

struct module {
	/* Metadata from JSON Config */
	char                   name[MODULE_MAX_NAME_LENGTH];
	char                   path[MODULE_MAX_PATH_LENGTH];
	uint32_t               stack_size; /* a specification? */
	uint64_t               max_memory; /* perhaps a specification of the module. (max 4GB) */
	uint32_t               relative_deadline_us;
	int                    port;
	struct admissions_info admissions_info;
	uint64_t               relative_deadline; /* cycles */

	/* Deferrable Server Attributes */
	uint64_t                 replenishment_period; /* cycles, not changing after init */
	uint64_t                 max_budget;           /* cycles, not changing after init */
	_Atomic volatile int64_t remaining_budget;     /* cycles left till next replenishment, can be negative */

	struct perworker_module_sandbox_queue *pwm_sandboxes;
	struct module_global_request_queue *   mgrq_requests;

	/* HTTP State */
	size_t             max_request_size;
	size_t             max_response_size;
	char               response_content_type[HTTP_MAX_HEADER_VALUE_LENGTH];
	struct sockaddr_in socket_address;
	int                socket_descriptor;

	/* Handle and ABI Symbols for *.so file */
	struct awsm_abi abi;

	_Atomic uint32_t            reference_count; /* ref count how many instances exist here. */
	struct indirect_table_entry indirect_table[INDIRECT_TABLE_SIZE];
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
 * Invoke a module's initialize_tables
 * @param module
 */
static inline void
module_initialize_table(struct module *module)
{
	module->abi.initialize_tables();
}

/**
 * Invoke a module's initialize_libc
 * @param module - module whose libc we are initializing
 * @param env - address?
 * @param arguments - address?
 */
static inline void
module_initialize_libc(struct module *module, int32_t env, int32_t arguments)
{
	module->abi.initialize_libc(env, arguments);
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
module_entrypoint(struct module *module, int32_t argc, int32_t argv)
{
	return module->abi.entrypoint(argc, argv);
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

/**
 * Get Timeout priority for Priority Queue ordering
 * @param element module_timeout
 * @returns the priority of the module _timeout element
 */
static inline uint64_t
module_timeout_get_priority(void *element)
{
	return ((struct module_timeout *)element)->timeout;
}

/**
 * Compute the next timeout given a module's replenishment period
 * @param m_replenishment_period
 * @return given module's next timeout
 */
static inline uint64_t
get_next_timeout_of_module(uint64_t m_replenishment_period, uint64_t now)
{
	// uint64_t now = __getcycles();
	return runtime_boot_timestamp
	       + ((now - runtime_boot_timestamp) / m_replenishment_period + 1) * m_replenishment_period;
}

/********************************
 * Public Methods from module.c *
 *******************************/

void module_free(struct module *module);

struct module *module_new(char *mod_name, char *mod_path, uint32_t stack_sz, uint32_t max_heap,
                          uint32_t relative_deadline_us, int port, int req_sz, int resp_sz, int admissions_percentile,
                          uint32_t expected_execution_us, uint32_t replenishment_period_us, uint32_t max_budget_us);

int module_new_from_json(char *filename);
