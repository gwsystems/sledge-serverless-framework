#include <threads.h>
#include <setjmp.h>
#include <threads.h>

#include "current_sandbox.h"
#include "sandbox_functions.h"
#include "sandbox_receive_request.h"
#include "sandbox_set_as_asleep.h"
#include "sandbox_set_as_error.h"
#include "sandbox_set_as_returned.h"
#include "sandbox_set_as_complete.h"
#include "sandbox_set_as_running_user.h"
#include "sandbox_set_as_running_sys.h"
#include "scheduler.h"
#include "software_interrupt.h"
#include "wasi.h"
#include "worker_thread_epoll.h"

thread_local struct sandbox *worker_thread_current_sandbox = NULL;

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

void
current_sandbox_wasm_trap_handler(int trapno)
{
	char           *error_message = NULL;
	struct sandbox *sandbox       = current_sandbox_get();
	sandbox_syscall(sandbox);

	struct http_session *session = sandbox->http;

	switch (trapno) {
	case WASM_TRAP_INVALID_INDEX:
		error_message = "WebAssembly Trap: Invalid Index\n";
		http_session_send_err(session, 500, current_sandbox_sleep);
		break;
	case WASM_TRAP_MISMATCHED_TYPE:
		error_message = "WebAssembly Trap: Mismatched Type\n";
		http_session_send_err(session, 500, current_sandbox_sleep);
		break;
	case WASM_TRAP_PROTECTED_CALL_STACK_OVERFLOW:
		error_message = "WebAssembly Trap: Protected Call Stack Overflow\n";
		http_session_send_err(session, 500, current_sandbox_sleep);
		break;
	case WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY:
		error_message = "WebAssembly Trap: Out of Bounds Linear Memory Access\n";
		http_session_send_err(session, 500, current_sandbox_sleep);
		break;
	case WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION:
		error_message = "WebAssembly Trap: Illegal Arithmetic Operation\n";
		http_session_send_err(session, 500, current_sandbox_sleep);
		break;
	case WASM_TRAP_UNREACHABLE:
		error_message = "WebAssembly Trap: Unreachable Instruction\n";
		http_session_send_err(session, 500, current_sandbox_sleep);
		break;
	default:
		error_message = "WebAssembly Trap: Unknown Trapno\n";
		http_session_send_err(session, 500, current_sandbox_sleep);
		break;
	}

	debuglog("%s", error_message);
	worker_thread_epoll_remove_sandbox(sandbox);
	http_session_close(sandbox->http);
	generic_thread_dump_lock_overhead();
	current_sandbox_exit();
	assert(0);
}


static inline struct sandbox *
current_sandbox_init()
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_RUNNING_SYS);

	int   rc            = 0;
	char *error_message = NULL;

	worker_thread_epoll_add_sandbox(sandbox);

	/* Initialize sandbox memory */
	struct module *current_module = sandbox_get_module(sandbox);
	module_initialize_memory(current_module);

	/* Initialize WASI */
	wasi_options_t options;
	wasi_options_init(&options);

	/* Initialize Arguments. First arg is the module name. Subsequent args are query parameters */
	char *args[HTTP_MAX_QUERY_PARAM_COUNT + 1];
	args[0] = sandbox->module->name;
	for (int i = 0; i < sandbox->http->http_request.query_params_count; i++)
		args[i + 1] = (char *)sandbox->http->http_request.query_params[i].value;

	options.argc                                          = sandbox->http->http_request.query_params_count + 1;
	options.argv                                          = (const char **)&args;
	sandbox->wasi_context                                 = wasi_context_init(&options);
	sledge_abi__current_wasm_module_instance.wasi_context = sandbox->wasi_context;
	assert(sandbox->wasi_context != NULL);

	sandbox_return(sandbox);

	/* Initialize sandbox globals. Needs to run in user state */
	module_initialize_globals(current_module);

	return sandbox;

err:
	debuglog("%s", error_message);
	worker_thread_epoll_remove_sandbox(sandbox);
	http_session_close(sandbox->http);
	generic_thread_dump_lock_overhead();
	current_sandbox_exit();
	return NULL;
}

extern noreturn void
current_sandbox_fini()
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);

	char *error_message = "";
	sandbox_syscall(sandbox);

	sandbox->timestamp_of.completion = __getcycles();
	sandbox->total_time              = sandbox->timestamp_of.completion - sandbox->timestamp_of.request_arrival;

	/* Retrieve the result, construct the HTTP response, and send to client */
	if (http_session_send_response(sandbox->http, sandbox->module->response_content_type, current_sandbox_sleep)
	    < 0) {
		error_message = "Unable to build and send client response\n";
		goto err;
	};

	http_total_increment_2xx();

	sandbox->timestamp_of.response = __getcycles();

	assert(sandbox->state == SANDBOX_RUNNING_SYS);
	worker_thread_epoll_remove_sandbox(sandbox);

done:
	http_session_close(sandbox->http);
	sandbox_set_as_returned(sandbox, SANDBOX_RUNNING_SYS);

	/* Cleanup connection and exit sandbox */
	generic_thread_dump_lock_overhead();
	current_sandbox_exit();
	assert(0);
err:
	debuglog("%s", error_message);
	assert(sandbox->state == SANDBOX_RUNNING_SYS);

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
	struct sandbox *sandbox = current_sandbox_init();

	int rc = sigsetjmp(sandbox->ctxt.start_buf, 1);
	if (rc == 0) {
		struct module *current_module = sandbox_get_module(sandbox);
		sandbox->return_value         = module_entrypoint(current_module);
	} else {
		current_sandbox_wasm_trap_handler(rc);
	}

	current_sandbox_fini();
}
