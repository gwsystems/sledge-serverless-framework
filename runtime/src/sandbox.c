#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>

#include "admissions_control.h"
#include "current_sandbox.h"
#include "debuglog.h"
#include "http_parser_settings.h"
#include "http_total.h"
#include "local_completion_queue.h"
#include "local_runqueue.h"
#include "likely.h"
#include "panic.h"
#include "runtime.h"
#include "sandbox.h"
#include "worker_thread.h"

/**
 * Takes the arguments from the sandbox struct and writes them into the WebAssembly linear memory
 */
static inline void
sandbox_setup_arguments(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	char *  arguments      = sandbox_get_arguments(sandbox);
	int32_t argument_count = module_get_argument_count(sandbox->module);

	/* whatever gregor has, to be able to pass arguments to a module! */
	sandbox->arguments_offset = local_sandbox_context_cache.linear_memory_size;
	assert(local_sandbox_context_cache.linear_memory_start == sandbox->linear_memory_start);
	expand_memory();

	int32_t *array_ptr  = worker_thread_get_memory_ptr_void(sandbox->arguments_offset,
                                                               argument_count * sizeof(int32_t));
	int32_t  string_off = sandbox->arguments_offset + (argument_count * sizeof(int32_t));

	for (int i = 0; i < argument_count; i++) {
		char * arg    = arguments + (i * MODULE_MAX_ARGUMENT_SIZE);
		size_t str_sz = strlen(arg) + 1;

		array_ptr[i] = string_off;
		/* why get_memory_ptr_for_runtime?? */
		strncpy(get_memory_ptr_for_runtime(string_off, str_sz), arg, strlen(arg));

		string_off += str_sz;
	}
	stub_init(string_off);
}

/**
 * Receive and Parse the Request for the current sandbox
 * @return 0 if message parsing complete, -1 on error
 */
static inline int
sandbox_receive_and_parse_client_request(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox->module->max_request_size > 0);
	assert(sandbox->request_response_data_length == 0);

	int rc = 0;

	while (!sandbox->http_request.message_end) {
		/* Read from the Socket */

		/* Structured to closely follow usage example at https://github.com/nodejs/http-parser */
		http_parser *               parser   = &sandbox->http_parser;
		const http_parser_settings *settings = http_parser_settings_get();

		int    fd  = sandbox->client_socket_descriptor;
		char * buf = &sandbox->request_response_data[sandbox->request_response_data_length];
		size_t len = sandbox->module->max_request_size - sandbox->request_response_data_length;

		ssize_t recved = recv(fd, buf, len, 0);

		if (recved < 0) {
			if (errno == EAGAIN) {
				worker_thread_block_current_sandbox();
				continue;
			} else {
				/* All other errors */
				debuglog("Error reading socket %d - %s\n", sandbox->client_socket_descriptor,
				         strerror(errno));
				goto err;
			}
		}

		/* Client request is malformed */
		if (recved == 0 && !sandbox->http_request.message_end) {
			char client_address_text[INET6_ADDRSTRLEN] = {};
			if (unlikely(inet_ntop(AF_INET, &sandbox->client_address, client_address_text, INET6_ADDRSTRLEN)
			             == NULL)) {
				debuglog("Failed to log client_address: %s", strerror(errno));
			}

			debuglog("Sandbox %lu: recv returned 0 before a complete request was received\n", sandbox->id);
			debuglog("Socket: %d. Address: %s\n", fd, client_address_text);
			http_request_print(&sandbox->http_request);
			goto err;
		}

#ifdef LOG_HTTP_PARSER
		debuglog("Sandbox: %lu http_parser_execute(%p, %p, %p, %zu\n)", sandbox->id, parser, settings, buf,
		         recved);
#endif
		size_t nparsed = http_parser_execute(parser, settings, buf, recved);

		if (nparsed != recved) {
			debuglog("Error: %s, Description: %s\n", http_errno_name(sandbox->http_parser.status_code),
			         http_errno_description(sandbox->http_parser.status_code));
			debuglog("Length Parsed %zu, Length Read %zu\n", nparsed, recved);
			debuglog("Error parsing socket %d\n", sandbox->client_socket_descriptor);
			goto err;
		}


		sandbox->request_response_data_length += nparsed;
	}


	sandbox->request_length = sandbox->request_response_data_length;

	rc = 0;
done:
	return rc;
err:
	rc = -1;
	goto done;
}

/**
 * Sends Response Back to Client
 * @return RC. -1 on Failure
 */
static inline int
sandbox_build_and_send_client_response(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	/*
	 * At this point the HTTP Request has filled the buffer up to request_length, after which
	 * the STDOUT of the sandbox has been appended. We assume that our HTTP Response header is
	 * smaller than the HTTP Request header, which allows us to use memmove once without copying
	 * to an intermediate buffer.
	 */
	memset(sandbox->request_response_data, 0, sandbox->request_length);

	/*
	 * We use this cursor to keep track of our position in the buffer and later assert that we
	 * haven't overwritten body data.
	 */
	size_t response_cursor = 0;

	/* Append 200 OK */
	strncpy(sandbox->request_response_data, HTTP_RESPONSE_200_OK, strlen(HTTP_RESPONSE_200_OK));
	response_cursor += strlen(HTTP_RESPONSE_200_OK);

	/* Content Type */
	strncpy(sandbox->request_response_data + response_cursor, HTTP_RESPONSE_CONTENT_TYPE,
	        strlen(HTTP_RESPONSE_CONTENT_TYPE));
	response_cursor += strlen(HTTP_RESPONSE_CONTENT_TYPE);

	/* Custom content type if provided, text/plain by default */
	if (strlen(sandbox->module->response_content_type) <= 0) {
		strncpy(sandbox->request_response_data + response_cursor, HTTP_RESPONSE_CONTENT_TYPE_PLAIN,
		        strlen(HTTP_RESPONSE_CONTENT_TYPE_PLAIN));
		response_cursor += strlen(HTTP_RESPONSE_CONTENT_TYPE_PLAIN);
	} else {
		strncpy(sandbox->request_response_data + response_cursor, sandbox->module->response_content_type,
		        strlen(sandbox->module->response_content_type));
		response_cursor += strlen(sandbox->module->response_content_type);
	}

	strncpy(sandbox->request_response_data + response_cursor, HTTP_RESPONSE_CONTENT_TYPE_TERMINATOR,
	        strlen(HTTP_RESPONSE_CONTENT_TYPE_TERMINATOR));
	response_cursor += strlen(HTTP_RESPONSE_CONTENT_TYPE_TERMINATOR);

	/* Content Length */
	strncpy(sandbox->request_response_data + response_cursor, HTTP_RESPONSE_CONTENT_LENGTH,
	        strlen(HTTP_RESPONSE_CONTENT_LENGTH));
	response_cursor += strlen(HTTP_RESPONSE_CONTENT_LENGTH);

	size_t body_size = sandbox->request_response_data_length - sandbox->request_length;

	char len[10] = { 0 };
	sprintf(len, "%zu", body_size);
	strncpy(sandbox->request_response_data + response_cursor, len, strlen(len));
	response_cursor += strlen(len);

	strncpy(sandbox->request_response_data + response_cursor, HTTP_RESPONSE_CONTENT_LENGTH_TERMINATOR,
	        strlen(HTTP_RESPONSE_CONTENT_LENGTH_TERMINATOR));
	response_cursor += strlen(HTTP_RESPONSE_CONTENT_LENGTH_TERMINATOR);

	/*
	 * Assumption: Our response header is smaller than the request header, so we do not overwrite
	 * actual data that the program appended to the HTTP Request. If proves to be a bad assumption,
	 * we have to copy the STDOUT string to a temporary buffer before writing the header
	 */
	if (unlikely(response_cursor >= sandbox->request_length)) {
		panic("Response Cursor: %zd is less that Request Length: %zd\n", response_cursor,
		      sandbox->request_length);
	}

	/* Move the Sandbox's Data after the HTTP Response Data */
	memmove(sandbox->request_response_data + response_cursor,
	        sandbox->request_response_data + sandbox->request_length, body_size);
	response_cursor += body_size;

	/* Capture Timekeeping data for end-to-end latency */
	uint64_t end_time      = __getcycles();
	sandbox->total_time    = end_time - sandbox->request_arrival_timestamp;
	uint64_t total_time_us = sandbox->total_time / runtime_processor_speed_MHz;

	int rc;
	int sent = 0;
	while (sent < response_cursor) {
		rc = write(sandbox->client_socket_descriptor, &sandbox->request_response_data[sent],
		           response_cursor - sent);
		if (rc < 0) {
			if (errno == EAGAIN)
				worker_thread_block_current_sandbox();
			else {
				perror("write");
				return -1;
			}
		}

		sent += rc;
	}

	http_total_increment_2xx();

	return 0;
}

static inline void
sandbox_open_http(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	http_parser_init(&sandbox->http_parser, HTTP_REQUEST);

	/* Set the sandbox as the data the http-parser has access to */
	sandbox->http_parser.data = sandbox;

	/* Freshly allocated sandbox going runnable for first time, so register client socket with epoll */
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)sandbox;
	accept_evt.events   = EPOLLIN | EPOLLOUT | EPOLLET;
	int rc = epoll_ctl(worker_thread_epoll_file_descriptor, EPOLL_CTL_ADD, sandbox->client_socket_descriptor,
	                   &accept_evt);
	if (unlikely(rc < 0)) panic_err();
}

/**
 * Initialize files descriptors 0, 1, and 2 as io handles 0, 1, 2
 * @param sandbox - the sandbox on which we are initializing file descriptors
 */
static inline void
sandbox_initialize_io_handles_and_file_descriptors(struct sandbox *sandbox)
{
	int f = sandbox_initialize_io_handle_and_set_file_descriptor(sandbox, 0);
	assert(f == 0);
	f = sandbox_initialize_io_handle_and_set_file_descriptor(sandbox, 1);
	assert(f == 1);
	f = sandbox_initialize_io_handle_and_set_file_descriptor(sandbox, 2);
	assert(f == 2);
}

/**
 * Sandbox execution logic
 * Handles setup, request parsing, WebAssembly initialization, function execution, response building and
 * sending, and cleanup
 */
void
current_sandbox_main(void)
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_RUNNING);

	int   rc;
	char *error_message = "";

	assert(!software_interrupt_is_enabled());
	arch_context_init(&sandbox->ctxt, 0, 0);
	software_interrupt_enable();

	sandbox_initialize_io_handles_and_file_descriptors(sandbox);

	sandbox_open_http(sandbox);

	/* Parse the request */
	rc = sandbox_receive_and_parse_client_request(sandbox);
	if (rc < 0) {
		error_message = "Unable to receive and parse client request\n";
		goto err;
	};

	/* Initialize the module */
	struct module *current_module = sandbox_get_module(sandbox);
	int            argument_count = module_get_argument_count(current_module);

	module_initialize_globals(current_module);
	module_initialize_memory(current_module);

	/* Copy the arguments into the WebAssembly sandbox */
	sandbox_setup_arguments(sandbox);

	/* Executing the function */
	sandbox->return_value         = module_main(current_module, argument_count, sandbox->arguments_offset);
	sandbox->completion_timestamp = __getcycles();

	/* Retrieve the result, construct the HTTP response, and send to client */
	rc = sandbox_build_and_send_client_response(sandbox);
	if (rc < 0) {
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
	worker_thread_on_sandbox_exit(sandbox);

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

/**
 * Allocates a WebAssembly sandbox represented by the following layout
 * struct sandbox | Buffer for HTTP Req/Resp | 4GB of Wasm Linear Memory | Guard Page
 * @param module the module that we want to run
 * @returns the resulting sandbox or NULL if mmap failed
 */
static inline struct sandbox *
sandbox_allocate_memory(struct module *module)
{
	assert(module != NULL);

	char *          error_message          = NULL;
	unsigned long   linear_memory_size     = WASM_PAGE_SIZE * WASM_START_PAGES; /* The initial pages */
	uint64_t        linear_memory_max_size = (uint64_t)SANDBOX_MAX_MEMORY;
	struct sandbox *sandbox                = NULL;
	unsigned long   sandbox_size           = sizeof(struct sandbox) + module->max_request_or_response_size;

	/*
	 * Control information should be page-aligned
	 * TODO: Should I use round_up_to_page when setting sandbox_page? Issue #50
	 */
	assert(round_up_to_page(sandbox_size) == sandbox_size);

	/* At an address of the system's choosing, allocate the memory, marking it as inaccessible */
	errno      = 0;
	void *addr = mmap(NULL, sandbox_size + linear_memory_max_size + /* guard page */ PAGE_SIZE, PROT_NONE,
	                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		error_message = "sandbox_allocate_memory - memory allocation failed";
		goto alloc_failed;
	}

	assert(addr != NULL);

	/* Set the struct sandbox, HTTP Req/Resp buffer, and the initial Wasm Pages as read/write */
	errno         = 0;
	void *addr_rw = mmap(addr, sandbox_size + linear_memory_size, PROT_READ | PROT_WRITE,
	                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (addr_rw == MAP_FAILED) {
		error_message = "set to r/w";
		goto set_rw_failed;
	}

	sandbox = (struct sandbox *)addr_rw;

	/* Populate Sandbox members */
	sandbox->state                  = SANDBOX_UNINITIALIZED;
	sandbox->linear_memory_start    = (char *)addr + sandbox_size;
	sandbox->linear_memory_size     = linear_memory_size;
	sandbox->linear_memory_max_size = linear_memory_max_size;
	sandbox->module                 = module;
	sandbox->sandbox_size           = sandbox_size;
	module_acquire(module);

done:
	return sandbox;
set_rw_failed:
	sandbox = NULL;
	errno   = 0;
	int rc  = munmap(addr, sandbox_size + linear_memory_size + PAGE_SIZE);
	if (rc == -1) perror("Failed to munmap after fail to set r/w");
alloc_failed:
err:
	perror(error_message);
	goto done;
}

int
sandbox_allocate_stack(struct sandbox *sandbox)
{
	assert(sandbox);
	assert(sandbox->module);
	assert(!software_interrupt_is_enabled());

	errno      = 0;
	char *addr = mmap(NULL, sandbox->module->stack_size + /* guard page */ PAGE_SIZE, PROT_NONE,
	                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) goto err_stack_allocation_failed;

	/* Set the struct sandbox, HTTP Req/Resp buffer, and the initial Wasm Pages as read/write */
	errno         = 0;
	char *addr_rw = mmap(addr + /* guard page */ PAGE_SIZE, sandbox->module->stack_size, PROT_READ | PROT_WRITE,
	                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

	/* TODO: Fix leak here. Issue #132 */
	if (addr_rw == MAP_FAILED) goto err_stack_allocation_failed;

	sandbox->stack_start = addr_rw;
	sandbox->stack_size  = sandbox->module->stack_size;

done:
	return 0;
err_stack_allocation_failed:
	perror("sandbox_allocate_stack");
	return -1;
}

/**
 * Transitions a sandbox to the SANDBOX_INITIALIZED state.
 * The sandbox was already zeroed out during allocation
 * @param sandbox an uninitialized sandbox
 * @param sandbox_request the request we are initializing the sandbox from
 * @param allocation_timestamp timestamp of allocation
 */
void
sandbox_set_as_initialized(struct sandbox *sandbox, struct sandbox_request *sandbox_request,
                           uint64_t allocation_timestamp)
{
	assert(!software_interrupt_is_enabled());
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_ALLOCATED);
	assert(sandbox_request != NULL);
	assert(allocation_timestamp > 0);

	sandbox->id                  = sandbox_request->id;
	sandbox->admissions_estimate = sandbox_request->admissions_estimate;

	sandbox->request_arrival_timestamp = sandbox_request->request_arrival_timestamp;
	sandbox->allocation_timestamp      = allocation_timestamp;
	sandbox->state                     = SANDBOX_SET_AS_INITIALIZED;

	/* Initialize the sandbox's context, stack, and instruction pointer */
	/* stack_start points to the bottom of the usable stack, so add stack_size to get to top */
	arch_context_init(&sandbox->ctxt, (reg_t)current_sandbox_main,
	                  (reg_t)sandbox->stack_start + sandbox->stack_size);

	/* Initialize file descriptors to -1 */
	for (int i = 0; i < SANDBOX_MAX_IO_HANDLE_COUNT; i++) sandbox->io_handles[i].file_descriptor = -1;

	/* Initialize Parsec control structures */
	ps_list_init_d(sandbox);

	/* Copy the socket descriptor, address, and arguments of the client invocation */
	sandbox->absolute_deadline        = sandbox_request->absolute_deadline;
	sandbox->arguments                = (void *)sandbox_request->arguments;
	sandbox->client_socket_descriptor = sandbox_request->socket_descriptor;
	memcpy(&sandbox->client_address, &sandbox_request->socket_address, sizeof(struct sockaddr));

	sandbox->last_state_change_timestamp = allocation_timestamp; /* We use arg to include alloc */
	sandbox->state                       = SANDBOX_INITIALIZED;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, SANDBOX_UNINITIALIZED, SANDBOX_INITIALIZED);
	runtime_sandbox_total_increment(SANDBOX_INITIALIZED);
}

/**
 * Transitions a sandbox to the SANDBOX_RUNNABLE state.
 *
 * This occurs in the following scenarios:
 * - A sandbox in the SANDBOX_INITIALIZED state completes initialization and is ready to be run
 * - A sandbox in the SANDBOX_BLOCKED state completes what was blocking it and is ready to be run
 *
 * @param sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
void
sandbox_set_as_runnable(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_RUNNABLE;

	switch (last_state) {
	case SANDBOX_INITIALIZED: {
		sandbox->initializing_duration += duration_of_last_state;
		break;
	}
	case SANDBOX_BLOCKED: {
		sandbox->blocked_duration += duration_of_last_state;
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Runnable\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	local_runqueue_add(sandbox);
	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RUNNABLE;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_RUNNABLE);
	runtime_sandbox_total_increment(SANDBOX_RUNNABLE);
	runtime_sandbox_total_decrement(last_state);
}

/**
 * Transitions a sandbox to the SANDBOX_RUNNING state.
 *
 * This occurs in the following scenarios:
 * - A sandbox is in a RUNNABLE state
 * 		- after initialization. This sandbox has thus not yet been executed
 * 		- after previously executing, blocking, waking up.
 * - A sandbox in the PREEMPTED state is now the highest priority work to execute
 *
 * @param sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
void
sandbox_set_as_running(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_RUNNING;

	switch (last_state) {
	case SANDBOX_RUNNABLE: {
		sandbox->runnable_duration += duration_of_last_state;
		break;
	}
	case SANDBOX_PREEMPTED: {
		sandbox->preempted_duration += duration_of_last_state;
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Running\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	current_sandbox_set(sandbox);
	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RUNNING;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_RUNNING);
	runtime_sandbox_total_increment(SANDBOX_RUNNING);
	runtime_sandbox_total_decrement(last_state);
}

/**
 * Transitions a sandbox to the SANDBOX_PREEMPTED state.
 *
 * This occurs when a sandbox is executing and in a RUNNING state and a SIGALRM software interrupt fires
 * and pulls a sandbox with an earlier absolute deadline from the global request scheduler.
 *
 * @param sandbox the sandbox being preempted
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
void
sandbox_set_as_preempted(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_PREEMPTED;

	switch (last_state) {
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Preempted\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_PREEMPTED;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_PREEMPTED);
	runtime_sandbox_total_increment(SANDBOX_PREEMPTED);
	runtime_sandbox_total_decrement(SANDBOX_RUNNING);
}

/**
 * Transitions a sandbox to the SANDBOX_BLOCKED state.
 * This occurs when a sandbox is executing and it makes a blocking API call of some kind.
 * Automatically removes the sandbox from the runqueue
 * @param sandbox the blocking sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
void
sandbox_set_as_blocked(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_BLOCKED;

	switch (last_state) {
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		local_runqueue_delete(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Blocked\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_BLOCKED;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_BLOCKED);
	runtime_sandbox_total_increment(SANDBOX_BLOCKED);
	runtime_sandbox_total_decrement(last_state);
}

/**
 * Transitions a sandbox to the SANDBOX_RETURNED state.
 * This occurs when a sandbox is executing and runs to completion.
 * Automatically removes the sandbox from the runqueue and unmaps linear memory.
 * Because the stack is still in use, freeing the stack is deferred until later
 * @param sandbox the blocking sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
void
sandbox_set_as_returned(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_RETURNED;

	switch (last_state) {
	case SANDBOX_RUNNING: {
		sandbox->response_timestamp = now;
		sandbox->total_time         = now - sandbox->request_arrival_timestamp;
		sandbox->running_duration += duration_of_last_state;
		local_runqueue_delete(sandbox);
		sandbox_free_linear_memory(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Returned\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RETURNED;

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox->id, last_state, SANDBOX_RETURNED);
	runtime_sandbox_total_increment(SANDBOX_RETURNED);
	runtime_sandbox_total_decrement(last_state);
}

/**
 * Transitions a sandbox to the SANDBOX_ERROR state.
 * This can occur during initialization or execution
 * Unmaps linear memory, removes from the runqueue (if on it), and adds to the completion queue
 * Because the stack is still in use, freeing the stack is deferred until later
 *
 * TODO: Is the sandbox adding itself to the completion queue here? Is this a problem? Issue #94
 *
 * @param sandbox the sandbox erroring out
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
void
sandbox_set_as_error(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_ERROR;

	switch (last_state) {
	case SANDBOX_SET_AS_INITIALIZED:
		/* Technically, this is a degenerate sandbox that we generate by hand */
		sandbox->initializing_duration += duration_of_last_state;
		break;
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		local_runqueue_delete(sandbox);
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Error\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	uint64_t sandbox_id = sandbox->id;
	sandbox->state      = SANDBOX_ERROR;
	sandbox_print_perf(sandbox);
#ifdef LOG_SANDBOX_MEMORY_PROFILE
	sandbox_summarize_page_allocations(sandbox);
#endif
	sandbox_free_linear_memory(sandbox);
	admissions_control_subtract(sandbox->admissions_estimate);
	/* Do not touch sandbox after adding to completion queue to avoid use-after-free bugs */
	local_completion_queue_add(sandbox);

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox_id, last_state, SANDBOX_ERROR);
	runtime_sandbox_total_increment(SANDBOX_ERROR);
	runtime_sandbox_total_decrement(last_state);
}

/**
 * Transitions a sandbox from the SANDBOX_RETURNED state to the SANDBOX_COMPLETE state.
 * Adds the sandbox to the completion queue
 * @param sandbox
 * @param last_state the state the sandbox is transitioning from. This is expressed as a constant to
 * enable the compiler to perform constant propagation optimizations.
 */
void
sandbox_set_as_complete(struct sandbox *sandbox, sandbox_state_t last_state)
{
	assert(sandbox);
	assert(!software_interrupt_is_enabled());

	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	sandbox->state = SANDBOX_SET_AS_COMPLETE;

	switch (last_state) {
	case SANDBOX_RETURNED: {
		sandbox->completion_timestamp = now;
		sandbox->returned_duration += duration_of_last_state;
		break;
	}
	default: {
		panic("Sandbox %lu | Illegal transition from %s to Error\n", sandbox->id,
		      sandbox_state_stringify(last_state));
	}
	}

	uint64_t sandbox_id = sandbox->id;
	sandbox->state      = SANDBOX_COMPLETE;
	sandbox_print_perf(sandbox);
#ifdef LOG_SANDBOX_MEMORY_PROFILE
	sandbox_summarize_page_allocations(sandbox);
#endif
	/* Admissions Control Post Processing */
	admissions_info_update(&sandbox->module->admissions_info, sandbox->running_duration);
	admissions_control_subtract(sandbox->admissions_estimate);
	/* Do not touch sandbox state after adding to completion queue to avoid use-after-free bugs */
	local_completion_queue_add(sandbox);

	/* State Change Bookkeeping */
	sandbox_state_log_transition(sandbox_id, last_state, SANDBOX_COMPLETE);
	runtime_sandbox_total_increment(SANDBOX_COMPLETE);
	runtime_sandbox_total_decrement(last_state);
}

/**
 * Allocates a new sandbox from a sandbox request
 * Frees the sandbox request on success
 * @param sandbox_request request being allocated
 * @returns sandbox * on success, NULL on error
 */
struct sandbox *
sandbox_allocate(struct sandbox_request *sandbox_request)
{
	/* Assumption: Caller has disabled software interrupts */
	assert(!software_interrupt_is_enabled());

	/* Validate Arguments */
	assert(sandbox_request != NULL);
	module_validate(sandbox_request->module);

	struct sandbox *sandbox;
	char *          error_message = "";
	uint64_t        now           = __getcycles();

	/* Allocate Sandbox control structures, buffers, and linear memory in a 4GB address space */
	sandbox = sandbox_allocate_memory(sandbox_request->module);
	if (!sandbox) {
		error_message = "failed to allocate sandbox heap and linear memory";
		goto err_memory_allocation_failed;
	}

	/* Allocate the Stack */
	if (sandbox_allocate_stack(sandbox) < 0) {
		error_message = "failed to allocate sandbox stack";
		goto err_stack_allocation_failed;
	}
	sandbox->state = SANDBOX_ALLOCATED;

	/* Set state to initializing */
	sandbox_set_as_initialized(sandbox, sandbox_request, now);

	free(sandbox_request);
done:
	return sandbox;
err_stack_allocation_failed:
	/*
	 * This is a degenerate sandbox that never successfully completed initialization, so we need to
	 * hand jam some things to be able to cleanly transition to ERROR state
	 */
	sandbox->state                       = SANDBOX_SET_AS_INITIALIZED;
	sandbox->last_state_change_timestamp = now;
#ifdef LOG_SANDBOX_MEMORY_PROFILE
	sandbox->page_allocation_timestamps_size = 0;
#endif
	ps_list_init_d(sandbox);
err_memory_allocation_failed:
err:
	sandbox_set_as_error(sandbox, SANDBOX_SET_AS_INITIALIZED);
	perror(error_message);
	sandbox = NULL;
	goto done;
}

/**
 * Free Linear Memory, leaving stack in place
 * @param sandbox
 */
void
sandbox_free_linear_memory(struct sandbox *sandbox)
{
	int rc = munmap(sandbox->linear_memory_start, SANDBOX_MAX_MEMORY + PAGE_SIZE);
	if (rc == -1) panic("sandbox_free_linear_memory - munmap failed\n");
	sandbox->linear_memory_start = NULL;
}

/**
 * Free stack and heap resources.. also any I/O handles.
 * @param sandbox
 */
void
sandbox_free(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox != current_sandbox_get());
	assert(sandbox->state == SANDBOX_ERROR || sandbox->state == SANDBOX_COMPLETE);

	char *error_message = NULL;
	int   rc;

	module_release(sandbox->module);

	/* Free Sandbox Stack */
	errno = 0;

	/* The stack start is the bottom of the usable stack, but we allocated a guard page below this */
	rc = munmap((char *)sandbox->stack_start - PAGE_SIZE, sandbox->stack_size + PAGE_SIZE);
	if (rc == -1) {
		debuglog("Failed to unmap stack of Sandbox %lu\n", sandbox->id);
		goto err_free_stack_failed;
	};


	/* Free Remaining Sandbox Linear Address Space
	 * sandbox_size includes the struct and HTTP buffer
	 * The linear memory was already freed during the transition from running to error|complete
	 * struct sandbox | HTTP Buffer | 4GB of Wasm Linear Memory | Guard Page
	 * Allocated      | Allocated   | Freed                     | Freed
	 */

	/* Linear Memory and Guard Page should already have been munmaped and set to NULL */
	assert(sandbox->linear_memory_start == NULL);
	errno = 0;
	rc    = munmap(sandbox, sandbox->sandbox_size);
	if (rc == -1) {
		debuglog("Failed to unmap Sandbox %lu\n", sandbox->id);
		goto err_free_sandbox_failed;
	};

done:
	return;
err_free_sandbox_failed:
err_free_stack_failed:
err:
	/* Errors freeing memory is a fatal error */
	panic("Failed to free Sandbox %lu\n", sandbox->id);
}
