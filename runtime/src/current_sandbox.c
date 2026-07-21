#include <setjmp.h>
#include <threads.h>

#include "current_sandbox.h"
#include "sandbox_functions.h"
#include "sandbox_set_as_asleep.h"
#include "sandbox_set_as_complete.h"
#include "sandbox_set_as_error.h"
#include "sandbox_set_as_returned.h"
#include "sandbox_set_as_running_sys.h"
#include "sandbox_set_as_running_user.h"
#include "scheduler.h"
#include "software_interrupt.h"
#include "wasi.h"

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
 * This places the current sandbox on the cleanup queue if in RETURNED or RUNNING_SYS state
 */
void
current_sandbox_exit()
{
	struct sandbox *exiting_sandbox = current_sandbox_get();
	assert(exiting_sandbox != NULL);

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

/**
 * @brief Exit from the executing sandbox during an interrup. 
 * 
 * Should be called within the signal handler (preemptive scheduler)
 * This places the current sandbox on the cleanup queue.
 */
void interrupted_sandbox_exit(void)
{
	struct sandbox *exiting_sandbox = current_sandbox_get();
	assert(exiting_sandbox != NULL);
	assert(exiting_sandbox->state == SANDBOX_INTERRUPTED);

	sandbox_exit_error(exiting_sandbox);
	local_cleanup_queue_add(exiting_sandbox);
}

void
current_sandbox_wasm_trap_handler(int trapno)
{
	char           *error_message = NULL;
	struct sandbox *sandbox       = current_sandbox_get();
	sandbox_syscall(sandbox);

	switch (trapno) {
	case WASM_TRAP_INVALID_INDEX:
		error_message = "WebAssembly Trap: Invalid Index\n";
		break;
	case WASM_TRAP_MISMATCHED_TYPE:
		error_message = "WebAssembly Trap: Mismatched Type\n";
		break;
	case WASM_TRAP_PROTECTED_CALL_STACK_OVERFLOW:
		error_message = "WebAssembly Trap: Protected Call Stack Overflow\n";
		break;
	case WASM_TRAP_OUT_OF_BOUNDS_LINEAR_MEMORY:
		error_message = "WebAssembly Trap: Out of Bounds Linear Memory Access\n";
		break;
	case WASM_TRAP_ILLEGAL_ARITHMETIC_OPERATION:
		error_message = "WebAssembly Trap: Illegal Arithmetic Operation\n";
		break;
	case WASM_TRAP_UNREACHABLE:
		error_message = "WebAssembly Trap: Unreachable Instruction\n";
		break;
	default:
		error_message = "WebAssembly Trap: Unknown Trapno\n";
		break;
	}

	// debuglog("%s - Tenant: %s, Route: %s", error_message, sandbox->tenant->name, sandbox->route->route);
	debuglog("%s - T: %s, id: %lu, exceeded: %u, rem_exec: %lu, premp: %u, state: %u, abi->size: %lu", error_message, sandbox->tenant->name, sandbox->id,
		sandbox->exceeded_estimation, sandbox->remaining_exec, sandbox->writeback_preemption_in_progress, sandbox->state, 
		sandbox->memory->abi.size);
	current_sandbox_exit();
	assert(0);
}


static inline struct sandbox *
current_sandbox_init()
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_RUNNING_SYS);

	// int   rc            = 0;
	// char *error_message = NULL;

	/* Initialize sandbox memory */
	struct module *current_module = sandbox_get_module(sandbox);
	module_initialize_memory(current_module);

	/* Initialize WASI */
	wasi_options_t options;
	wasi_options_init(&options);

	/* Initialize Arguments. First arg is the module name. Subsequent args are query parameters */
	char *args[HTTP_MAX_QUERY_PARAM_COUNT + 1];
	args[0] = sandbox->module->path;
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

// err:
// 	debuglog("%s", error_message);
// 	current_sandbox_exit();
// 	return NULL;
}

extern noreturn void
current_sandbox_fini()
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);

	char *error_message = "";
	sandbox_syscall(sandbox);

	// sandbox->timestamp_of.completion = __getcycles();
	// sandbox->total_time              = sandbox->timestamp_of.completion - sandbox->timestamp_of.allocation;

	assert(sandbox->state == SANDBOX_RUNNING_SYS);

done:
	sandbox_set_as_returned(sandbox, SANDBOX_RUNNING_SYS);

	/* Cleanup connection and exit sandbox */
	current_sandbox_exit();
	assert(0);
err:
	assert(0);
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

	if (sandbox->module->type == APP_MODULE) current_sandbox_fini();
}

int
sandbox_validate_self_lifetime(struct sandbox *sandbox)
{
	if (sandbox->response_code != 0) goto err;

	const uint64_t now = __getcycles();
	if (sandbox->absolute_deadline >= now + (!sandbox->exceeded_estimation ? sandbox->remaining_exec : 0)) return 0;

	// dbf_try_update_demand(worker_dbf, sandbox->timestamp_of.dispatched,
	// 		                      sandbox->route->relative_deadline, sandbox->absolute_deadline, sandbox->remaining_exec,
	// 		                      DBF_DELETE_EXISTING_DEMAND, NULL, NULL);

	assert(sandbox->response_code == 0);
	sandbox->response_code = 4081;

err:	
	sandbox_exit_error(sandbox);
	// sandbox_free(sandbox);
	// sandbox_free_linear_memory(sandbox);
	local_cleanup_queue_add(sandbox);
	return -1;
}
