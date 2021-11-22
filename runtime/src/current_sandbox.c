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
#include "sandbox_set_as_running_sys.h"
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
	struct sandbox *sleeping_sandbox = current_sandbox_get();
	assert(sleeping_sandbox != NULL);

	generic_thread_dump_lock_overhead();

	switch (sleeping_sandbox->state) {
	case SANDBOX_RUNNING_SYS: {
		sandbox_sleep(sleeping_sandbox);
		break;
	}
	default:
		panic("Cooperatively switching from a sandbox in a non-terminal %s state\n",
		      sandbox_state_stringify(sleeping_sandbox->state));
	}

	scheduler_cooperative_sched(false);
}

/**
 * @brief Switches from an executing sandbox to the worker thread base context
 *
 * This places the current sandbox on the completion queue if in RETURNED state
 */
void
current_sandbox_exit()
{
	struct sandbox *exiting_sandbox = current_sandbox_get();
	assert(exiting_sandbox != NULL);

	generic_thread_dump_lock_overhead();

	switch (exiting_sandbox->state) {
	case SANDBOX_RETURNED:
		sandbox_exit_success(exiting_sandbox);
		break;
	case SANDBOX_RUNNING_SYS:
		sandbox_exit_error(exiting_sandbox);
		break;
	default:
		panic("Cooperatively switching from a sandbox in a non-terminal %s state\n",
		      sandbox_state_stringify(exiting_sandbox->state));
	}

	scheduler_cooperative_sched(true);

	/* The scheduler should never switch back to completed sandboxes */
	assert(0);
}

static inline struct sandbox *
current_sandbox_init()
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_RUNNING_SYS);

	int rc = 0;

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
	sandbox_return(sandbox);

	return sandbox;

err:
	sandbox_close_http(sandbox);
	generic_thread_dump_lock_overhead();
	current_sandbox_exit();
	assert(0);

	return NULL;
}

static inline void
current_sandbox_fini()
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);

	char *error_message = "";
	sandbox_syscall(sandbox);

	sandbox->timestamp_of.completion = __getcycles();

	/* Retrieve the result, construct the HTTP response, and send to client */
	if (sandbox_send_response(sandbox) < 0) {
		error_message = "Unable to build and send client response\n";
		goto err;
	};

	http_total_increment_2xx();

	sandbox->timestamp_of.response = __getcycles();

	assert(sandbox->state == SANDBOX_RUNNING_SYS);
	sandbox_close_http(sandbox);
	sandbox_set_as_returned(sandbox, SANDBOX_RUNNING_SYS);

done:
	/* Cleanup connection and exit sandbox */
	generic_thread_dump_lock_overhead();
	current_sandbox_exit();
	assert(0);
err:
	debuglog("%s", error_message);
	assert(sandbox->state == SANDBOX_RUNNING_SYS);

	sandbox_close_http(sandbox);
	goto done;
}

/**
 * Sandbox execution logic
 * Handles setup, request parsing, WebAssembly initialization, function execution, response building and
 * sending, and cleanup
 */
void
current_sandbox_start(void)
{
	struct sandbox *sandbox        = current_sandbox_init();
	struct module * current_module = sandbox_get_module(sandbox);
	int32_t         argument_count = 0;
	sandbox->return_value          = module_entrypoint(current_module, argument_count, sandbox->arguments_offset);
	current_sandbox_fini();
}
