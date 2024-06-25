#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debuglog.h"
#include "http.h"
#include "likely.h"
#include "listener_thread.h"
#include "module.h"
#include "module_database.h"
#include "panic.h"
#include "runtime.h"
#include "scheduler.h"
#include "tcp_server.h"
#include "wasm_table.h"

/*************************
 * Private Static Inline *
 ************************/

/**
 * Initializes a module
 *
 * @param module
 * @param path passes ownership of string to the allocated module if successful
 * @returns 0 on success, -1 on error
 */
static inline int
module_init(struct module *module, char *path)
{
	assert(module != NULL);
	assert(path != NULL);
	assert(strlen(path) > 0);

	uint32_t stack_size = 0;

	int rc = 0;

	atomic_init(&module->reference_count, 0);

	rc = sledge_abi_symbols_init(&module->abi, path);
	if (rc != 0) goto err;

	const int n = module->type == APP_MODULE ? runtime_worker_threads_count : 1;
	// module->pools = calloc(n, sizeof(struct module_pool));
	module->pools = calloc(1, sizeof(struct module_pool));

	module->path = path;

	module->stack_size = ((uint32_t)(round_up_to_page(stack_size == 0 ? WASM_STACK_SIZE : stack_size)));

	module_alloc_table(module);
	module_initialize_pools(module);
done:
	return rc;
err:
	rc = -1;
	goto done;
}

static inline void
module_deinit(struct module *module)
{
	assert(module == NULL);
	assert(module->reference_count == 0);

	free(module->path);
	sledge_abi_symbols_deinit(&module->abi);
	/* TODO: Free indirect_table */
	module_deinitialize_pools(module);
	free(module->pools);
}

/***************************************
 * Public Methods
 ***************************************/

/**
 * Module Mega Teardown Function
 * Closes the socket and dynamic library, and then frees the module
 * Returns harmlessly if there are outstanding references
 *
 * TODO: Untested Functionality. Unsure if this will work. Also, what about the module database? Do we
 * need to do any cleanup there? Issue #17
 * @param module - the module to teardown
 */
void
module_free(struct module *module)
{
	if (module == NULL) return;
	assert(module->reference_count == 0);

	panic("Unimplemented!\n");

	module_deinit(module);
	free(module);
}

/**
 * Module Contructor
 * Allocates and initializes a new module
 *
 * @param path passes ownership of string to the allocated module if successful
 * @returns A new module or NULL in case of failure
 */

struct module *
module_alloc(char *path, enum module_type type)
{
	size_t alignment     = (size_t)CACHE_PAD;
	size_t size_to_alloc = (size_t)round_to_cache_pad(sizeof(struct module));

	assert(size_to_alloc % alignment == 0);

	struct module *module = aligned_alloc(alignment, size_to_alloc);
	if (!module) {
		fprintf(stderr, "Failed to allocate module: %s\n", strerror(errno));
		goto err;
	};

	memset(module, 0, size_to_alloc);
	module->type = type;

	int rc = module_init(module, path);
	if (rc < 0) goto init_err;

done:
	return module;

init_err:
	free(module);
err:
	module = NULL;
	goto done;
}
