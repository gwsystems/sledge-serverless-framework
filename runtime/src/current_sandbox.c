#include <threads.h>

#include "current_sandbox.h"
#include "sandbox_functions.h"
#include "sandbox_receive_request.h"
#include "sandbox_send_response.h"
#include "sandbox_set_as_asleep.h"
#include "sandbox_set_as_error.h"
#include "sandbox_set_as_returned.h"
#include "sandbox_set_as_complete.h"
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
 * @brief Switches from an executing sandbox to the worker thread base context
 *
 * This places the current sandbox on the completion queue if in RETURNED state
 */
void
current_sandbox_sleep()
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	struct arch_context *current_context = &sandbox->ctxt;

	scheduler_log_sandbox_switch(sandbox, NULL);
	generic_thread_dump_lock_overhead();

	assert(sandbox != NULL);

	switch (sandbox->state) {
	case SANDBOX_RUNNING_KERNEL: {
		sandbox_sleep(sandbox);
		break;
	}
	default:
		panic("Cooperatively switching from a sandbox in a non-terminal %s state\n",
		      sandbox_state_stringify(sandbox->state));
	}

	current_sandbox_set(NULL);

	/* Assumption: Base Worker context should never be preempted */
	assert(worker_thread_base_context.variant == ARCH_CONTEXT_VARIANT_FAST);
	arch_context_switch(current_context, &worker_thread_base_context);
}

/**
 * @brief Switches from an executing sandbox to the worker thread base context
 *
 * This places the current sandbox on the completion queue if in RETURNED state
 */
void
current_sandbox_exit()
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	struct arch_context *current_context = &sandbox->ctxt;

	scheduler_log_sandbox_switch(sandbox, NULL);
	generic_thread_dump_lock_overhead();

	assert(sandbox != NULL);

	switch (sandbox->state) {
	case SANDBOX_RETURNED:
		/*
		 * We draw a distinction between RETURNED and COMPLETED because a sandbox cannot add itself to the
		 * completion queue
		 * TODO: I think this executes when running inside the sandbox, as it hasn't yet yielded
		 * See Issue #224 at https://github.com/gwsystems/sledge-serverless-framework/issues/224
		 */
		sandbox_set_as_complete(sandbox, SANDBOX_RETURNED);
		break;
	case SANDBOX_ERROR:
		break;
	default:
		panic("Cooperatively switching from a sandbox in a non-terminal %s state\n",
		      sandbox_state_stringify(sandbox->state));
	}

	current_sandbox_set(NULL);

	/* Assumption: Base Worker context should never be preempted */
	assert(worker_thread_base_context.variant == ARCH_CONTEXT_VARIANT_FAST);
	arch_context_switch(current_context, &worker_thread_base_context);
}


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
	current_sandbox_exit();

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
