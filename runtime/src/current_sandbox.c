#include "current_sandbox.h"
#include "sandbox_functions.h"
#include "sandbox_receive_request.h"
#include "sandbox_send_response.h"
#include "sandbox_set_as_error.h"
#include "sandbox_set_as_returned.h"
#include "sandbox_setup_arguments.h"
#include "scheduler.h"
#include "module.h"
#include "software_interrupt.h"

__thread struct sandbox *worker_thread_current_sandbox = NULL;

__thread struct sandbox_context_cache local_sandbox_context_cache = {
	.linear_memory_start   = NULL,
	.linear_memory_size    = 0,
	.module_indirect_table = NULL,
};

static inline void
current_sandbox_enable_preemption(struct sandbox *sandbox)
{
#ifdef LOG_PREEMPTION
	debuglog("Sandbox %lu - enabling preemption - Missed %d SIGALRM\n", sandbox->id,
	         software_interrupt_deferred_sigalrm);
	fflush(stderr);
#endif
	if (__sync_bool_compare_and_swap(&sandbox->ctxt.preemptable, 0, 1) == false) {
		panic("Recursive call to current_sandbox_enable_preemption\n");
	}

	if (software_interrupt_deferred_sigalrm > 0) {
		/* Update Max */
		if (software_interrupt_deferred_sigalrm > software_interrupt_deferred_sigalrm_max[worker_thread_idx]) {
			software_interrupt_deferred_sigalrm_max[worker_thread_idx] =
			  software_interrupt_deferred_sigalrm;
		}

		software_interrupt_deferred_sigalrm = 0;
		// TODO: Replay. Does the replay need to be before or after enabling preemption?
	}
}

static inline void
current_sandbox_disable_preemption(struct sandbox *sandbox)
{
#ifdef LOG_PREEMPTION
	debuglog("Sandbox %lu - disabling preemption\n", sandbox->id);
	fflush(stderr);
#endif
	if (__sync_bool_compare_and_swap(&sandbox->ctxt.preemptable, 1, 0) == false) {
		panic("Recursive call to current_sandbox_disable_preemption\n");
	}
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
	assert(sandbox->state == SANDBOX_RUNNING);

	char *error_message = "";

	sandbox_initialize_stdio(sandbox);
	struct module * next_module = sandbox->module->next_module;

	/* 
	 * Add the client fd to epoll if it is the first or last sandbox in the chain because they 
	 * need to read and write from/to this fd
	 */
	if (sandbox->request_from_outside || next_module == NULL) {
		sandbox_open_http(sandbox);
	}

	if (sandbox->request_from_outside) {
		if (sandbox_receive_request(sandbox) < 0) { 
			error_message = "Unable to receive or parse client request\n";
			goto err;
		}
	} else {
		/* 
		 * Copy previous output to sandbox->request_response_data, as the input for the current sandbox.
		 * Let sandbox->http_request->body points to sandbox->request_response_data
		 */
		assert(sandbox->previous_function_output != NULL);
		memcpy(sandbox->request_response_data, sandbox->previous_function_output, sandbox->output_length);
		sandbox->http_request.body 		= sandbox->request_response_data;
		sandbox->http_request.body_length 	= sandbox->output_length;
		sandbox->request_length 		= sandbox->previous_request_length;
		sandbox->request_response_data_length 	= sandbox->request_length;
	}

	/* Initialize sandbox memory */
	struct module *current_module = sandbox_get_module(sandbox);
	module_initialize_globals(current_module);
	module_initialize_memory(current_module);
	sandbox_setup_arguments(sandbox);

	/* Executing the function */
	int32_t argument_count = module_get_argument_count(current_module);
	current_sandbox_enable_preemption(sandbox);
	sandbox->return_value = module_main(current_module, argument_count, sandbox->arguments_offset);
	current_sandbox_disable_preemption(sandbox);
	sandbox->completion_timestamp = __getcycles();


	if (next_module != NULL) {
		/* Generate a new request, copy the current sandbox's output to the next request's buffer, and put it to the global queue */
		ssize_t output_length = sandbox->request_response_data_length - sandbox->request_length;
		char * pre_func_output = (char *)malloc(output_length);
		if (!pre_func_output) {
                	fprintf(stderr, "Failed to allocate memory for the previous output: %s\n", strerror(errno));
                	goto err;
        	};

		memcpy(pre_func_output, sandbox->request_response_data + sandbox->request_length, output_length);
	 	uint64_t enqueue_timestamp = __getcycles();	
		struct sandbox_request *sandbox_request =
                                  sandbox_request_allocate(next_module, false, sandbox->request_length, 
							   next_module->name, sandbox->client_socket_descriptor,
                                                           (const struct sockaddr *)&sandbox->client_address,
                                                           sandbox->request_arrival_timestamp, enqueue_timestamp, 
							   sandbox->remaining_slack, true, pre_func_output, output_length);
		/* TODO: All sandboxs in the chain share the same request id, but sandbox_request_allocate() 
		 *	 will busy-wait to generate an unique id, should we optimize it here?
		 */
		sandbox_request->id = sandbox->id; 
#ifdef OPT_AVOID_GLOBAL_QUEUE
		/* TODO: The running time of the current sandbox contains the next sandbox's initialization time, does it matter? */
		if (sandbox->absolute_deadline == sandbox_request->absolute_deadline) { 
			/* Put the next sandbox to the local run queue to reduce the overhead of the global queue */
			struct sandbox *next_sandbox = sandbox_allocate(sandbox_request);
                	if (!next_sandbox) {
				free(sandbox_request);
				goto err;
			}

                	assert(next_sandbox->state == SANDBOX_INITIALIZED);
                	sandbox_set_as_runnable(next_sandbox, SANDBOX_INITIALIZED);
		} else {
			/* Add to the Global Sandbox Request Scheduler */
                	global_request_scheduler_add(sandbox_request);
		}
#else 
		/* Add to the Global Sandbox Request Scheduler */
		global_request_scheduler_add(sandbox_request);
#endif
		/* Remove the client fd from epoll if it is the first sandbox in the chain */
		if (sandbox->request_from_outside) {
			sandbox_remove_from_epoll(sandbox);
		}
		sandbox_set_as_returned(sandbox, SANDBOX_RUNNING);
	} else {
		/* Retrieve the result, construct the HTTP response, and send to client */
		if (sandbox_send_response(sandbox) < 0) {
			error_message = "Unable to build and send client response\n";
			goto err;
		};

		http_total_increment_2xx();

		sandbox->response_timestamp = __getcycles();

		assert(sandbox->state == SANDBOX_RUNNING);
		sandbox_close_http(sandbox);
		sandbox_set_as_returned(sandbox, SANDBOX_RUNNING);
	}
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
	assert(sandbox->state == SANDBOX_RUNNING);

	/* Send a 400 error back to the client */
	client_socket_send(sandbox->client_socket_descriptor, 400);

	sandbox_close_http(sandbox);
	sandbox_set_as_error(sandbox, SANDBOX_RUNNING);
	goto done;
}
