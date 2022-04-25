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

static inline int
module_init(struct module *module, struct module_config *config)
{
	assert(module != NULL);
	assert(config != NULL);
	assert(config->name != NULL);
	assert(config->path != NULL);
	assert(config->http_resp_content_type != NULL);

	uint32_t stack_size = 0;

	/* Validate presence of required fields */
	if (strlen(config->name) == 0) panic("name field is required\n");
	if (strlen(config->path) == 0) panic("path field is required\n");
	if (config->port == 0) panic("port field is required\n");

	if (config->http_req_size > RUNTIME_HTTP_REQUEST_SIZE_MAX)
		panic("request_size must be between 0 and %u, was %u\n", (uint32_t)RUNTIME_HTTP_REQUEST_SIZE_MAX,
		      config->http_req_size);

	if (config->http_resp_size > RUNTIME_HTTP_RESPONSE_SIZE_MAX)
		panic("response-size must be between 0 and %u, was %u\n", (uint32_t)RUNTIME_HTTP_RESPONSE_SIZE_MAX,
		      config->http_resp_size);

	/* If a route is not specified, default to root */
	if (config->route == NULL) config->route = "/";

	struct module *existing_module = module_database_find_by_name(config->name);
	if (existing_module != NULL) panic("Module %s is already initialized\n", existing_module->name);

	existing_module = module_database_find_by_port(config->port);
	if (existing_module != NULL)
		panic("Module %s is already configured with port %u\n", existing_module->name, config->port);

	if (config->relative_deadline_us > (uint32_t)RUNTIME_RELATIVE_DEADLINE_US_MAX)
		panic("Relative-deadline-us must be between 0 and %u, was %u\n",
		      (uint32_t)RUNTIME_RELATIVE_DEADLINE_US_MAX, config->relative_deadline_us);

#ifdef ADMISSIONS_CONTROL
	/* expected-execution-us and relative-deadline-us are required in case of admissions control */
	if (config->expected_execution_us == 0) panic("expected-execution-us is required\n");
	if (config->relative_deadline_us == 0) panic("relative_deadline_us is required\n");

	if (config->admissions_percentile > 99 || config->admissions_percentile < 50)
		panic("admissions-percentile must be > 50 and <= 99 but was %u\n", config->admissions_percentile);

	/* If the ratio is too big, admissions control is too coarse */
	uint32_t ratio = config->relative_deadline_us / config->expected_execution_us;
	if (ratio > ADMISSIONS_CONTROL_GRANULARITY)
		panic("Ratio of Deadline to Execution time cannot exceed admissions control "
		      "granularity of "
		      "%d\n",
		      ADMISSIONS_CONTROL_GRANULARITY);
#else
	/* relative-deadline-us is required if scheduler is EDF */
	if (scheduler == SCHEDULER_EDF && config->relative_deadline_us == 0)
		panic("relative_deadline_us is required\n");
#endif

	int rc = 0;

	atomic_init(&module->reference_count, 0);

	rc = sledge_abi_symbols_init(&module->abi, config->path);
	if (rc != 0) goto err;

	/* Set fields in the module struct */
	strncpy(module->name, config->name, MODULE_MAX_NAME_LENGTH);
	strncpy(module->path, config->path, MODULE_MAX_PATH_LENGTH);
	strncpy(module->route, config->route, MODULE_MAX_ROUTE_LENGTH);
	strncpy(module->response_content_type, config->http_resp_content_type, HTTP_MAX_HEADER_VALUE_LENGTH);

	module->stack_size = ((uint32_t)(round_up_to_page(stack_size == 0 ? WASM_STACK_SIZE : stack_size)));

	tcp_server_init(&module->tcp_server, config->port);

	/* Deadlines */
	module->relative_deadline_us = config->relative_deadline_us;

	/* This can overflow a uint32_t, so be sure to cast appropriately */
	module->relative_deadline = (uint64_t)config->relative_deadline_us * runtime_processor_speed_MHz;

	/* Admissions Control */
	uint64_t expected_execution = (uint64_t)config->expected_execution_us * runtime_processor_speed_MHz;
	admissions_info_initialize(&module->admissions_info, config->admissions_percentile, expected_execution,
	                           module->relative_deadline);

	/* Request Response Buffer */
	if (config->http_req_size == 0) config->http_req_size = MODULE_DEFAULT_REQUEST_RESPONSE_SIZE;
	if (config->http_resp_size == 0) config->http_resp_size = MODULE_DEFAULT_REQUEST_RESPONSE_SIZE;
	module->max_request_size  = round_up_to_page(config->http_req_size);
	module->max_response_size = round_up_to_page(config->http_resp_size);

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
 * Start the module as a server listening at module->port
 * @param module
 * @returns 0 on success, -1 on error
 */
int
module_listen(struct module *module)
{
	int rc = tcp_server_listen(&module->tcp_server);
	if (rc < 0) goto err;

	/* Set the socket descriptor and register with our global epoll instance to monitor for incoming HTTP
	requests */
	rc = listener_thread_register_module(module);
	if (unlikely(rc < 0)) goto err_add_to_epoll;

	rc = 0;
done:
	return rc;
err_add_to_epoll:
	tcp_server_close(&module->tcp_server);
err:
	rc = -1;
	goto done;
}

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

	/* Do not free if we still have oustanding references */
	if (module->reference_count) return;

	tcp_server_close(&module->tcp_server);
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
module_alloc(struct module_config *config)
{
	struct module *module = (struct module *)calloc(1, sizeof(struct module));
	if (!module) {
		fprintf(stderr, "Failed to allocate module: %s\n", strerror(errno));
		goto err;
	};

	int rc = module_init(module, config);
	if (rc < 0) goto init_err;

done:
	return module;

init_err:
	free(module);
err:
	module = NULL;
	goto done;
}
