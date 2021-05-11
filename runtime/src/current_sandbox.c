#include "current_sandbox.h"
#include "current_sandbox_yield.h"
#include "sandbox_functions.h"
#include "sandbox_receive_request.h"
#include "sandbox_send_response.h"
#include "sandbox_set_as_error.h"
#include "sandbox_set_as_returned.h"
#include "sandbox_setup_arguments.h"

// /* current sandbox that is active.. */
__thread struct sandbox *worker_thread_current_sandbox = NULL;

__thread struct sandbox_context_cache local_sandbox_context_cache = {
	.linear_memory_start   = NULL,
	.linear_memory_size    = 0,
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
	assert(sandbox->state == SANDBOX_RUNNING);

	char *error_message = "";

	assert(!software_interrupt_is_enabled());
	arch_context_init(&sandbox->ctxt, 0, 0);
	software_interrupt_enable();

	sandbox_initialize_stdio(sandbox);

	sandbox_open_http(sandbox);

	if (sandbox_receive_request(sandbox) < 0) {
		error_message = "Unable to receive or parse client request\n";
		goto err;
	};

	/* Initialize sandbox memory */
	struct module *current_module = sandbox_get_module(sandbox);
	module_initialize_globals(current_module);
	module_initialize_memory(current_module);
	sandbox_setup_arguments(sandbox);

	/* Executing the function */
	int32_t argument_count        = module_get_argument_count(current_module);
	sandbox->return_value         = module_main(current_module, argument_count, sandbox->arguments_offset);
	sandbox->completion_timestamp = __getcycles();

	/* Retrieve the result, construct the HTTP response, and send to client */
	if (sandbox_send_response(sandbox) < 0) {
		error_message = "Unable to build and send client response\n";
		goto err;
	};

	http_total_increment_2xx();

	sandbox->response_timestamp = __getcycles();

	software_interrupt_disable();

	assert(sandbox->state == SANDBOX_RUNNING);
	sandbox_close_http(sandbox);
	sandbox_set_as_returned(sandbox, SANDBOX_RUNNING);

done:
	/* Cleanup connection and exit sandbox */
	generic_thread_dump_lock_overhead();
	current_sandbox_yield();

	/* This assert prevents a segfault discussed in
	 * https://github.com/phanikishoreg/awsm-Serverless-Framework/issues/66
	 */
	assert(0);
err:
	debuglog("%s", error_message);
	assert(sandbox->state == SANDBOX_RUNNING);

	/* Send a 400 error back to the client */
	client_socket_send(sandbox->client_socket_descriptor, 400);

	software_interrupt_disable();
	sandbox_close_http(sandbox);
	sandbox_set_as_error(sandbox, SANDBOX_RUNNING);
	goto done;
}
