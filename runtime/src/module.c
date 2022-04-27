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

#define IN

/*************************
 * Private Static Inline *
 ************************/

static inline int
module_init(struct module *module, IN char *path)
{
	assert(module != NULL);
	assert(path != NULL);
	assert(strlen(path) > 0);

	uint32_t stack_size = 0;

	int rc = 0;

	atomic_init(&module->reference_count, 0);

	rc = sledge_abi_symbols_init(&module->abi, path);
	if (rc != 0) goto err;

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

	panic("Unimplemented!\n");

	/* TODO: Should allocating routes increment reference */
	/* Do not free if we still have oustanding references */
	if (module->reference_count) return;

	sledge_abi_symbols_deinit(&module->abi);
	free(module);
}

/**
 * Module Contructor
 * Creates a new module, invokes initialize_tables to initialize the indirect table, and adds it to the module DB
 *
 * @param name
 * @param path
 * @param stack_size
 * @param relative_deadline_us
 * @param port
 * @param request_size
 * @returns A new module or NULL in case of failure
 */

struct module *
module_alloc(char *path)
{
	struct module *module = (struct module *)calloc(1, sizeof(struct module));
	if (!module) {
		fprintf(stderr, "Failed to allocate module: %s\n", strerror(errno));
		goto err;
	};

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
