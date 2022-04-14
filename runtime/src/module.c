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
#include "wasm_table.h"

/*************************
 * Private Static Inline *
 ************************/

/**
 * Start the module as a server listening at module->port
 * @param module
 * @returns 0 on success, -1 on error
 */
static inline int
module_listen(struct module *module)
{
	int rc;

	/* Allocate a new TCP/IP socket, setting it to be non-blocking */
	int socket_descriptor = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (unlikely(socket_descriptor < 0)) goto err_create_socket;

	/* Socket should never have returned on fd 0, 1, or 2 */
	assert(socket_descriptor != STDIN_FILENO);
	assert(socket_descriptor != STDOUT_FILENO);
	assert(socket_descriptor != STDERR_FILENO);

	/* Configure the socket to allow multiple sockets to bind to the same host and port */
	int optval = 1;
	rc         = setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	if (unlikely(rc < 0)) goto err_set_socket_option;
	optval = 1;
	rc     = setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (unlikely(rc < 0)) goto err_set_socket_option;

	/* Bind name [all addresses]:[module->port] to socket */
	module->socket_descriptor              = socket_descriptor;
	module->socket_address.sin_family      = AF_INET;
	module->socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
	module->socket_address.sin_port        = htons((unsigned short)module->port);
	rc = bind(socket_descriptor, (struct sockaddr *)&module->socket_address, sizeof(module->socket_address));
	if (unlikely(rc < 0)) goto err_bind_socket;

	/* Listen to the interface */
	rc = listen(socket_descriptor, MODULE_MAX_PENDING_CLIENT_REQUESTS);
	if (unlikely(rc < 0)) goto err_listen;


	/* Set the socket descriptor and register with our global epoll instance to monitor for incoming HTTP
	requests */
	rc = listener_thread_register_module(module);
	if (unlikely(rc < 0)) goto err_add_to_epoll;

	rc = 0;
done:
	return rc;
err_add_to_epoll:
err_listen:
err_bind_socket:
	module->socket_descriptor = -1;
err_set_socket_option:
	close(socket_descriptor);
err_create_socket:
err:
	debuglog("Socket Error: %s", strerror(errno));
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

	/* Do not free if we still have oustanding references */
	if (module->reference_count) return;

	close(module->socket_descriptor);
	sledge_abi_symbols_deinit(&module->abi);
	free(module);
}

static inline int
module_init(struct module *module, char *name, char *path, uint32_t stack_size, uint32_t relative_deadline_us,
            uint16_t port, uint32_t request_size, uint32_t response_size, uint8_t admissions_percentile,
            uint32_t expected_execution_us, char *response_content_type)
{
	assert(module != NULL);

	/* Validate presence of required fields */
	if (strlen(name) == 0) panic("name field is required\n");
	if (strlen(path) == 0) panic("path field is required\n");
	if (port == 0) panic("port field is required\n");

	if (relative_deadline_us > (uint32_t)RUNTIME_RELATIVE_DEADLINE_US_MAX)
		panic("Relative-deadline-us must be between 0 and %u, was %u\n",
		      (uint32_t)RUNTIME_RELATIVE_DEADLINE_US_MAX, relative_deadline_us);

	if (request_size > RUNTIME_HTTP_REQUEST_SIZE_MAX)
		panic("request_size must be between 0 and %u, was %u\n", (uint32_t)RUNTIME_HTTP_REQUEST_SIZE_MAX,
		      request_size);

	if (response_size > RUNTIME_HTTP_RESPONSE_SIZE_MAX)
		panic("response-size must be between 0 and %u, was %u\n", (uint32_t)RUNTIME_HTTP_RESPONSE_SIZE_MAX,
		      response_size);

#ifdef ADMISSIONS_CONTROL
	/* expected-execution-us and relative-deadline-us are required in case of admissions control */
	if (expected_execution_us == 0) panic("expected-execution-us is required\n");
	if (relative_deadline_us == 0) panic("relative_deadline_us is required\n");

	if (admissions_percentile > 99 || admissions_percentile < 50)
		panic("admissions-percentile must be > 50 and <= 99 but was %u\n", admissions_percentile);

	/* If the ratio is too big, admissions control is too coarse */
	uint32_t ratio = relative_deadline_us / expected_execution_us;
	if (ratio > ADMISSIONS_CONTROL_GRANULARITY)
		panic("Ratio of Deadline to Execution time cannot exceed admissions control "
		      "granularity of "
		      "%d\n",
		      ADMISSIONS_CONTROL_GRANULARITY);
#else
	/* relative-deadline-us is required if scheduler is EDF */
	if (scheduler == SCHEDULER_EDF && relative_deadline_us == 0) panic("relative_deadline_us is required\n");
#endif

	int rc = 0;

	atomic_init(&module->reference_count, 0);

	rc = sledge_abi_symbols_init(&module->abi, path);
	if (rc != 0) goto err;

	/* Set fields in the module struct */
	strncpy(module->name, name, MODULE_MAX_NAME_LENGTH);
	strncpy(module->path, path, MODULE_MAX_PATH_LENGTH);
	strncpy(module->response_content_type, response_content_type, HTTP_MAX_HEADER_VALUE_LENGTH);

	module->stack_size        = ((uint32_t)(round_up_to_page(stack_size == 0 ? WASM_STACK_SIZE : stack_size)));
	module->socket_descriptor = -1;
	module->port              = port;

	/* Deadlines */
	module->relative_deadline_us = relative_deadline_us;

	/* This can overflow a uint32_t, so be sure to cast appropriately */
	module->relative_deadline = (uint64_t)relative_deadline_us * runtime_processor_speed_MHz;

	/* Admissions Control */
	uint64_t expected_execution = (uint64_t)expected_execution_us * runtime_processor_speed_MHz;
	admissions_info_initialize(&module->admissions_info, admissions_percentile, expected_execution,
	                           module->relative_deadline);

	/* Request Response Buffer */
	if (request_size == 0) request_size = MODULE_DEFAULT_REQUEST_RESPONSE_SIZE;
	if (response_size == 0) response_size = MODULE_DEFAULT_REQUEST_RESPONSE_SIZE;
	module->max_request_size  = round_up_to_page(request_size);
	module->max_response_size = round_up_to_page(response_size);

	module_alloc_table(module);
	module_initialize_pools(module);
done:
	return rc;
err:
	rc = -1;
	goto done;
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
module_alloc(char *name, char *path, uint32_t stack_size, uint32_t relative_deadline_us, uint16_t port,
             uint32_t request_size, uint32_t response_size, uint8_t admissions_percentile,
             uint32_t expected_execution_us, char *response_content_type)
{
	struct module *module = (struct module *)calloc(1, sizeof(struct module));
	if (!module) {
		fprintf(stderr, "Failed to allocate module: %s\n", strerror(errno));
		goto err;
	};

	int rc = module_init(module, name, path, stack_size, relative_deadline_us, port, request_size, response_size,
	                     admissions_percentile, expected_execution_us, response_content_type);
	if (rc < 0) goto init_err;

done:
	return module;

init_err:
	free(module);
err:
	module = NULL;
	goto done;
}
