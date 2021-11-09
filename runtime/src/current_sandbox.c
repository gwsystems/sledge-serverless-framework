#include <threads.h>

#include "current_sandbox.h"
#include "sandbox_functions.h"
#include "sandbox_receive_request.h"
#include "sandbox_send_response.h"
#include "sandbox_set_as_error.h"
#include "sandbox_set_as_returned.h"
#include "sandbox_set_as_running_user.h"
#include "sandbox_set_as_running_kernel.h"
#include "sandbox_setup_arguments.h"
#include "scheduler.h"
#include "software_interrupt.h"

thread_local struct sandbox *worker_thread_current_sandbox = NULL;

thread_local struct sandbox_context_cache local_sandbox_context_cache = {
	.memory = {
		.start = NULL,
		.size = 0,
		.max = 0,
	},
	.module_indirect_table = NULL,
};

/**
 * Sandbox execution logic
 * Handles setup, request parsing, WebAssembly initialization, function execution, response building and
 * sending, and cleanup
 */
void
current_sandbox_start(void)
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_RUNNING_KERNEL);

	char *error_message = "";
	int   rc            = 0;

	sandbox_open_http(sandbox);

	rc = sandbox_receive_request(sandbox);
	if (rc == -2) {
		/* Request size exceeded Buffer, send 413 Payload Too Large */
		client_socket_send(sandbox->client_socket_descriptor, 413);
		goto err;
	} else if (rc == -1) {
		client_socket_send(sandbox->client_socket_descriptor, 400);
		goto err;
	}

	/* Initialize sandbox memory */
	struct module *current_module = sandbox_get_module(sandbox);
	module_initialize_globals(current_module);
	module_initialize_memory(current_module);
	sandbox_setup_arguments(sandbox);

	/* Executing the function */
	int32_t argument_count = 0;
	assert(sandbox->state == SANDBOX_RUNNING_KERNEL);
	sandbox_set_as_running_user(sandbox, SANDBOX_RUNNING_KERNEL);
	sandbox->return_value = module_entrypoint(current_module, argument_count, sandbox->arguments_offset);
	assert(sandbox->state == SANDBOX_RUNNING_USER);
	sandbox_set_as_running_kernel(sandbox, SANDBOX_RUNNING_USER);
	sandbox->timestamp_of.completion = __getcycles();

	/* Retrieve the result, construct the HTTP response, and send to client */
	if (sandbox_send_response(sandbox) < 0) {
		error_message = "Unable to build and send client response\n";
		goto err;
	};

	http_total_increment_2xx();

	sandbox->timestamp_of.response = __getcycles();

	assert(sandbox->state == SANDBOX_RUNNING_KERNEL);
	sandbox_close_http(sandbox);
	sandbox_set_as_returned(sandbox, SANDBOX_RUNNING_KERNEL);

done:
	/* Cleanup connection and exit sandbox */
	generic_thread_dump_lock_overhead();
	scheduler_yield();

	/* This assert prevents a segfault discussed in
	 * https://github.com/phanikishoreg/awsm-Serverless-Framework/issues/66
	 */
	assert(0);
err:
	debuglog("%s", error_message);
	assert(sandbox->state == SANDBOX_RUNNING_KERNEL);

	sandbox_close_http(sandbox);
	sandbox_set_as_error(sandbox, SANDBOX_RUNNING_KERNEL);
	goto done;
}
