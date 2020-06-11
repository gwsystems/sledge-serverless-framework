#include <assert.h>
#include <runtime.h>
#include <worker_thread.h>
#include <sandbox.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>
#include <uv.h>
#include <libuv_callbacks.h>
#include <current_sandbox.h>
#include <http_parser_settings.h>
#include <sandbox_completion_queue.h>
#include <sandbox_run_queue.h>

/**
 * Takes the arguments from the sandbox struct and writes them into the WebAssembly linear memory
 **/
static inline void
sandbox_setup_arguments(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	char *  arguments      = sandbox_get_arguments(sandbox);
	int32_t argument_count = module_get_argument_count(sandbox->module);

	// whatever gregor has, to be able to pass arguments to a module!
	sandbox->arguments_offset = sandbox_lmbound;
	assert(sandbox_lmbase == sandbox->linear_memory_start);
	expand_memory();

	int32_t *array_ptr  = worker_thread_get_memory_ptr_void(sandbox->arguments_offset,
                                                               argument_count * sizeof(int32_t));
	int32_t  string_off = sandbox->arguments_offset + (argument_count * sizeof(int32_t));

	for (int i = 0; i < argument_count; i++) {
		char * arg    = arguments + (i * MODULE_MAX_ARGUMENT_SIZE);
		size_t str_sz = strlen(arg) + 1;

		array_ptr[i] = string_off;
		// why get_memory_ptr_for_runtime??
		strncpy(get_memory_ptr_for_runtime(string_off, str_sz), arg, strlen(arg));

		string_off += str_sz;
	}
	stub_init(string_off);
}

/**
 * Run the http-parser on the sandbox's request_response_data using the configured settings global
 * @param sandbox the sandbox containing the req_resp data that we want to parse
 * @param length The size of the request_response_data that we want to parse
 * @returns 0
 *
 **/
int
sandbox_parse_http_request(struct sandbox *sandbox, size_t length)
{
	assert(sandbox != NULL);
	assert(length > 0);
	// Why is our start address sandbox->request_response_data + sandbox->request_response_data_length?
	// it's like a cursor to keep track of what we've read so far
	http_parser_execute(&sandbox->http_parser, http_parser_settings_get(),
	                    sandbox->request_response_data + sandbox->request_response_data_length, length);
	return 0;
}

/**
 * Receive and Parse the Request for the current sandbox
 * @return 1 on success, 0 if no context, < 0 on failure.
 **/
static inline int
sandbox_receive_and_parse_client_request(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	sandbox->request_response_data_length = 0;
#ifndef USE_HTTP_UVIO
	int r = 0;
	r = recv(sandbox->client_socket_descriptor, (sandbox->request_response_data), sandbox->module->max_request_size,
	         0);
	if (r <= 0) {
		if (r < 0) perror("recv1");
		return r;
	}
	while (r > 0) {
		if (sandbox_parse_http_request(sandbox, r) != 0) return -1;
		sandbox->request_response_data_length += r;
		struct http_request *rh = &sandbox->http_request;
		if (rh->message_end) break;

		r = recv(sandbox->client_socket_descriptor,
		         (sandbox->request_response_data + sandbox->request_response_data_length),
		         sandbox->module->max_request_size - sandbox->request_response_data_length, 0);
		if (r < 0) {
			perror("recv2");
			return r;
		}
	}
#else
	int r = uv_read_start((uv_stream_t *)&sandbox->client_libuv_stream,
	                      libuv_callbacks_on_allocate_setup_request_response_data,
	                      libuv_callbacks_on_read_parse_http_request);
	worker_thread_process_io();
	if (sandbox->request_response_data_length == 0) return 0;
#endif
	return 1;
}

/**
 * Sends Response Back to Client
 * @return RC. -1 on Failure
 **/
static inline int
sandbox_build_and_send_client_response(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	int sndsz                  = 0;
	int response_header_length = strlen(HTTP_RESPONSE_200_OK) + strlen(HTTP_RESPONSE_CONTENT_TYPE)
	                             + strlen(HTTP_RESPONSE_CONTENT_LENGTH);
	int body_length = sandbox->request_response_data_length - response_header_length;

	memset(sandbox->request_response_data, 0,
	       strlen(HTTP_RESPONSE_200_OK) + strlen(HTTP_RESPONSE_CONTENT_TYPE)
	         + strlen(HTTP_RESPONSE_CONTENT_LENGTH));
	strncpy(sandbox->request_response_data, HTTP_RESPONSE_200_OK, strlen(HTTP_RESPONSE_200_OK));
	sndsz += strlen(HTTP_RESPONSE_200_OK);

	if (body_length == 0) goto done;
	strncpy(sandbox->request_response_data + sndsz, HTTP_RESPONSE_CONTENT_TYPE, strlen(HTTP_RESPONSE_CONTENT_TYPE));
	if (strlen(sandbox->module->response_content_type) <= 0) {
		strncpy(sandbox->request_response_data + sndsz + strlen("Content-type: "),
		        HTTP_RESPONSE_CONTENT_TYPE_PLAIN, strlen(HTTP_RESPONSE_CONTENT_TYPE_PLAIN));
	} else {
		strncpy(sandbox->request_response_data + sndsz + strlen("Content-type: "),
		        sandbox->module->response_content_type, strlen(sandbox->module->response_content_type));
	}
	sndsz += strlen(HTTP_RESPONSE_CONTENT_TYPE);
	char len[10] = { 0 };
	sprintf(len, "%d", body_length);
	strncpy(sandbox->request_response_data + sndsz, HTTP_RESPONSE_CONTENT_LENGTH,
	        strlen(HTTP_RESPONSE_CONTENT_LENGTH));
	strncpy(sandbox->request_response_data + sndsz + strlen("Content-length: "), len, strlen(len));
	sndsz += strlen(HTTP_RESPONSE_CONTENT_LENGTH);
	sndsz += body_length;

done:
	assert(sndsz == sandbox->request_response_data_length);

#ifndef USE_HTTP_UVIO
	int r = send(sandbox->client_socket_descriptor, sandbox->request_response_data, sndsz, 0);
	if (r < 0) {
		perror("send");
		return -1;
	}
	while (r < sndsz) {
		int s = send(sandbox->client_socket_descriptor, sandbox->request_response_data + r, sndsz - r, 0);
		if (s < 0) {
			perror("send");
			return -1;
		}
		r += s;
	}
#else
	uv_write_t req = {
		.data = sandbox,
	};
	uv_buf_t bufv = uv_buf_init(sandbox->request_response_data, sndsz);
	int      r    = uv_write(&req, (uv_stream_t *)&sandbox->client_libuv_stream, &bufv, 1,
                         libuv_callbacks_on_write_wakeup_sandbox);
	worker_thread_process_io();
#endif
	return 0;
}

static inline void
sandbox_close_http(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

#ifdef USE_HTTP_UVIO
	uv_close((uv_handle_t *)&sandbox->client_libuv_stream, libuv_callbacks_on_close_wakeup_sakebox);
	worker_thread_process_io();
#else
	close(sandbox->client_socket_descriptor);
#endif
}

static inline void
sandbox_open_http(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	http_parser_init(&sandbox->http_parser, HTTP_REQUEST);

	// Set the sandbox as the data the http-parser has access to
	sandbox->http_parser.data = sandbox;

#ifdef USE_HTTP_UVIO

	// Initialize libuv TCP stream
	int r = uv_tcp_init(worker_thread_get_libuv_handle(), (uv_tcp_t *)&sandbox->client_libuv_stream);
	assert(r == 0);

	// Set the current sandbox as the data the libuv callbacks have access to
	sandbox->client_libuv_stream.data = sandbox;

	// Open the libuv TCP stream
	r = uv_tcp_open((uv_tcp_t *)&sandbox->client_libuv_stream, sandbox->client_socket_descriptor);
	assert(r == 0);
#endif
}

// Initialize file descriptors 0, 1, and 2 as io handles 0, 1, 2
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

char *
sandbox_get_state(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	switch (sandbox->state) {
	case SANDBOX_INITIALIZING:
		return "Initializing";
	case SANDBOX_RUNNABLE:
		return "Runnable";
	case SANDBOX_RUNNING:
		return "Running";
	case SANDBOX_BLOCKED:
		return "Blocked";
	case SANDBOX_RETURNED:
		return "Returned";
	case SANDBOX_COMPLETE:
		return "Complete";
	case SANDBOX_ERROR:
		return "Error";
	default:
		// Crash, as this should be exclusive
		assert(0);
	}
}

/**
 * Sandbox execution logic
 * Handles setup, request parsing, WebAssembly initialization, function execution, response building and sending, and
 *cleanup
 **/
void
current_sandbox_main(void)
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	if (sandbox->state != SANDBOX_RUNNING) {
		printf("Sandbox Unexpectedly transitioning from %s\n", sandbox_get_state(sandbox));
		assert(0);
	};

	char *error_message = "";

	assert(!software_interrupt_is_enabled());
	arch_context_init(&sandbox->ctxt, 0, 0);
	worker_thread_next_context = NULL;
	software_interrupt_enable();

	sandbox_initialize_io_handles_and_file_descriptors(sandbox);

	sandbox_open_http(sandbox);

	// Parse the request. 1 = Success
	int rc = sandbox_receive_and_parse_client_request(sandbox);
	if (rc != 1) {
		error_message = "Unable to receive and parse client request";
		goto err;
	};

	// Initialize the module
	struct module *current_module = sandbox_get_module(sandbox);
	int            argument_count = module_get_argument_count(current_module);
	// alloc_linear_memory();
	module_initialize_globals(current_module);
	module_initialize_memory(current_module);

	// Copy the arguments into the WebAssembly sandbox
	sandbox_setup_arguments(sandbox);

	// Executing the function
	sandbox->return_value         = module_main(current_module, argument_count, sandbox->arguments_offset);
	sandbox->completion_timestamp = __getcycles();

	// Retrieve the result, construct the HTTP response, and send to client
	rc = sandbox_build_and_send_client_response(sandbox);
	if (rc == -1) {
		error_message = "Unable to build and send client response";
		goto err;
	};

	sandbox->response_timestamp = __getcycles();

	software_interrupt_disable();
	sandbox_set_as_returned(sandbox);
	software_interrupt_enable();

done:
	// Cleanup connection and exit sandbox
	sandbox_close_http(sandbox);
	worker_thread_on_sandbox_exit(sandbox);
	assert(0);
	return;
err:
	perror(error_message);
	sandbox_set_as_error(sandbox);
	goto done;
}

/**
 * Allocates a WebAssembly sandbox represented by the following layout
 * struct sandbox | Buffer for HTTP Req/Resp | 4GB of Wasm Linear Memory | Guard Page
 * @param module the module that we want to run
 * @returns the resulting sandbox or NULL if mmap failed
 **/
static inline struct sandbox *
sandbox_allocate_memory(struct module *module)
{
	assert(module != NULL);

	char *          error_message          = NULL;
	unsigned long   linear_memory_size     = WASM_PAGE_SIZE * WASM_START_PAGES; // The initial pages
	uint64_t        linear_memory_max_size = (uint64_t)SBOX_MAX_MEM;
	struct sandbox *sandbox                = NULL;
	unsigned long   sandbox_size           = sizeof(struct sandbox) + module->max_request_or_response_size;

	// Control information should be page-aligned
	// TODO: Should I use round_up_to_page when setting sandbox_page?
	assert(round_up_to_page(sandbox_size) == sandbox_size);

	// At an address of the system's choosing, allocate the memory, marking it as inaccessible
	errno      = 0;
	void *addr = mmap(NULL, sandbox_size + linear_memory_max_size + /* guard page */ PAGE_SIZE, PROT_NONE,
	                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		error_message = "sandbox_allocate_memory - memory allocation failed";
		goto alloc_failed;
	}
	sandbox = (struct sandbox *)addr;

	// Set the struct sandbox, HTTP Req/Resp buffer, and the initial Wasm Pages as read/write
	errno         = 0;
	void *addr_rw = mmap(addr, sandbox_size + linear_memory_size, PROT_READ | PROT_WRITE,
	                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (addr_rw == MAP_FAILED) {
		error_message = "set to r/w";
		goto set_rw_failed;
	}

	// Populate Sandbox members
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
alloc_too_big:
err:
	perror(error_message);
	goto done;
}

int
sandbox_allocate_stack(sandbox_t *sandbox)
{
	assert(sandbox);
	assert(sandbox->module);

	errno                = 0;
	sandbox->stack_size  = sandbox->module->stack_size;
	sandbox->stack_start = mmap(NULL, sandbox->stack_size, PROT_READ | PROT_WRITE,
	                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN, -1, 0);
	if (sandbox->stack_start == MAP_FAILED) goto err_stack_allocation_failed;

done:
	return 0;
err_stack_allocation_failed:
	perror("sandbox_allocate_stack");
	return -1;
}

/**
 * Prints key performance metrics for a sandbox to STDOUT
 * @param sandbox
 **/
static inline void
sandbox_print_perf(sandbox_t *sandbox)
{
	uint64_t total_time_us = sandbox->total_time / runtime_processor_speed_MHz;
	uint64_t queued_us = (sandbox->allocation_timestamp - sandbox->request_timestamp) / runtime_processor_speed_MHz;
	uint64_t initializing_us = sandbox->initializing_duration / runtime_processor_speed_MHz;
	uint64_t runnable_us     = sandbox->runnable_duration / runtime_processor_speed_MHz;
	uint64_t running_us      = sandbox->running_duration / runtime_processor_speed_MHz;
	uint64_t blocked_us      = sandbox->blocked_duration / runtime_processor_speed_MHz;
	uint64_t returned_us     = sandbox->returned_duration / runtime_processor_speed_MHz;
	printf("%s():%d, state: %s, deadline: %u, actual: %lu, queued: %lu, initializing: %lu, runnable: %lu, running: "
	       "%lu, blocked: "
	       "%lu, returned %lu\n",
	       sandbox->module->name, sandbox->module->port, sandbox_get_state(sandbox),
	       sandbox->module->relative_deadline_us, total_time_us, queued_us, initializing_us, runnable_us,
	       running_us, blocked_us, returned_us);
}

/**
 * Transitions a sandbox to the SANDBOX_INITIALIZING state. Because this is the initial state of a new sandbox, we have
 * to assume that sandbox->state is garbage.
 *
 * TODO: Consider zeroing out allocation of the sandbox struct to be able to assert that we are only calling this on a
 *clean allocation. Additionally, we might want to shift the sandbox states up, so zero is distinct from
 *SANDBOX_INITIALIZING
 * @param sandbox
 **/
void
sandbox_set_as_initializing(sandbox_t *sandbox)
{
	assert(sandbox);
	uint64_t now                         = __getcycles();
	sandbox->allocation_timestamp        = now;
	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_INITIALIZING;
}

/**
 * Transitions a sandbox to the SANDBOX_RUNNABLE state.
 *
 * This occurs in the following scenarios:
 * - A sandbox in the SANDBOX_INITIALIZING state completes initialization and is ready to be run
 * - A sandbox in the SANDBOX_BLOCKED state completes what was blocking it and is ready to be run
 * - A sandbox in the SANDBOX_RUNNING state is preempted before competion and is ready to be run
 *
 * @param sandbox
 * @param running_sandbox_context - Optionally save the current context of a sandbox in the SANDBOX_RUNNING state
 **/
void
sandbox_set_as_runnable(sandbox_t *sandbox, const mcontext_t *running_sandbox_context)
{
	assert(sandbox);
	assert(sandbox->last_state_change_timestamp > 0);
	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	switch (sandbox->state) {
	case SANDBOX_INITIALIZING: {
		assert(running_sandbox_context == NULL);
		sandbox->initializing_duration += duration_of_last_state;
		sandbox_run_queue_add(sandbox);
		break;
	}
	case SANDBOX_BLOCKED: {
		assert(running_sandbox_context == NULL);
		sandbox->blocked_duration += duration_of_last_state;
		sandbox_run_queue_add(sandbox);
		break;
	}
	case SANDBOX_RUNNING: {
		// TODO: How to handle the "switch logic"
		// assert(running_sandbox_context != NULL);
		if (running_sandbox_context != NULL) arch_mcontext_save(&sandbox->ctxt, running_sandbox_context);
		sandbox->running_duration += duration_of_last_state;
		break;
	}
	default: {
		printf("Sandbox Unexpectedly transitioning from %s to Runnable\n", sandbox_get_state(sandbox));
		assert(0);
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RUNNABLE;
}

void
sandbox_set_as_running(sandbox_t *sandbox)
{
	assert(sandbox);
	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	switch (sandbox->state) {
	case SANDBOX_RUNNABLE: {
		sandbox->runnable_duration += duration_of_last_state;
		current_sandbox_set(sandbox);
		break;
	}
	default: {
		printf("Sandbox Unexpectedly transitioning from %s to Running\n", sandbox_get_state(sandbox));
		assert(0);
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RUNNING;
}

void
sandbox_set_as_blocked(sandbox_t *sandbox)
{
	assert(sandbox);
	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	switch (sandbox->state) {
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		sandbox_run_queue_delete(sandbox);
		break;
	}
	default: {
		printf("Sandbox Unexpectedly transitioning from %s to Blocked\n", sandbox_get_state(sandbox));
		assert(0);
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_BLOCKED;
}

// Because the stack is still in use, only unmap linear memory and defer free resources until
// "main function execution"
void
sandbox_set_as_returned(sandbox_t *sandbox)
{
	assert(sandbox);
	uint64_t now                    = __getcycles();
	uint64_t duration_of_last_state = now - sandbox->last_state_change_timestamp;

	switch (sandbox->state) {
	case SANDBOX_RUNNING: {
		sandbox->response_timestamp = now;
		sandbox->total_time         = now - sandbox->request_timestamp;
		sandbox->running_duration += duration_of_last_state;
		sandbox_run_queue_delete(sandbox);
		sandbox_free_linear_memory(sandbox);
		break;
	}
	default: {
		printf("Sandbox Unexpectedly transitioning from %s to Returned\n", sandbox_get_state(sandbox));
		assert(0);
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RETURNED;
}

void
sandbox_set_as_error(sandbox_t *sandbox)
{
	assert(sandbox);
	uint64_t now                            = __getcycles();
	uint64_t duration_of_last_state         = now - sandbox->last_state_change_timestamp;
	bool     should_add_to_completion_queue = false;

	switch (sandbox->state) {
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		sandbox_run_queue_delete(sandbox);
		sandbox_free_linear_memory(sandbox);
		should_add_to_completion_queue = true;
		break;
	}
	default: {
		printf("Sandbox Unexpectedly transitioning from %s to Error\n", sandbox_get_state(sandbox));
		assert(0);
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_ERROR;

	// Note: We defer this until the end of Do not touch sandbox state after adding to the completion queue to
	// avoid use-after-free bugs
	if (should_add_to_completion_queue) {
		sandbox_print_perf(sandbox);
		sandbox_completion_queue_add(sandbox);
	};
}

void
sandbox_set_as_complete(sandbox_t *sandbox)
{
	assert(sandbox);
	uint64_t now                            = __getcycles();
	uint64_t duration_of_last_state         = now - sandbox->last_state_change_timestamp;
	bool     should_add_to_completion_queue = false;

	switch (sandbox->state) {
	case SANDBOX_RETURNED: {
		sandbox->completion_timestamp = now;
		sandbox->returned_duration += duration_of_last_state;
		should_add_to_completion_queue = true;
		break;
	}
	default: {
		printf("Sandbox Unexpectedly transitioning from %s to Complete\n", sandbox_get_state(sandbox));
		assert(0);
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_COMPLETE;

	// Note: We defer this until the end of Do not touch sandbox state after adding to the completion queue to
	// avoid use-after-free bugs
	if (should_add_to_completion_queue) {
		sandbox_print_perf(sandbox);
		sandbox_completion_queue_add(sandbox);
	};
}

struct sandbox *
sandbox_allocate(sandbox_request_t *sandbox_request)
{
	assert(sandbox_request != NULL);
	assert(sandbox_request->module != NULL);
	assert(module_is_valid(sandbox_request->module));
	uint64_t now = __getcycles();

	char *          error_message = "";
	int             rc;
	struct sandbox *sandbox = NULL;

	// Allocate Sandbox control structures, buffers, and linear memory in a 4GB address space
	errno   = 0;
	sandbox = (struct sandbox *)sandbox_allocate_memory(sandbox_request->module);
	if (!sandbox) goto err_memory_allocation_failed;

	sandbox_set_as_initializing(sandbox);

	// Allocate the Stack
	rc = sandbox_allocate_stack(sandbox);
	if (rc != 0) {
		error_message = "failed to allocate sandbox stack";
		goto err_stack_allocation_failed;
	};

	// Copy the socket descriptor, address, and arguments of the client invocation
	sandbox->absolute_deadline = sandbox_request->absolute_deadline;
	sandbox->total_time        = 0;

	sandbox->arguments                = (void *)sandbox_request->arguments;
	sandbox->client_socket_descriptor = sandbox_request->socket_descriptor;

	sandbox->request_timestamp    = sandbox_request->request_timestamp;
	sandbox->allocation_timestamp = now;
	sandbox->response_timestamp   = 0;
	sandbox->completion_timestamp = 0;

	sandbox->initializing_duration = 0;
	sandbox->runnable_duration     = 0;
	sandbox->running_duration      = 0;
	sandbox->blocked_duration      = 0;
	sandbox->returned_duration     = 0;

	// Initialize the sandbox's context, stack, and instruction pointer
	arch_context_init(&sandbox->ctxt, (reg_t)current_sandbox_main,
	                  (reg_t)(sandbox->stack_start + sandbox->stack_size));

	// What does it mean if there isn't a socket_address? Shouldn't this be a hard requirement?
	// It seems that only the socket descriptor is used to send response
	const struct sockaddr *socket_address = sandbox_request->socket_address;
	if (socket_address) memcpy(&sandbox->client_address, socket_address, sizeof(struct sockaddr));

	// Initialize file descriptors to -1
	for (int i = 0; i < SANDBOX_MAX_IO_HANDLE_COUNT; i++) sandbox->io_handles[i].file_descriptor = -1;

	// Initialize Parsec control structures (used by Completion Queue)
	ps_list_init_d(sandbox);

done:
	return sandbox;
err_stack_allocation_failed:
	sandbox_set_as_error(sandbox);
err_memory_allocation_failed:
err:
	perror(error_message);
	assert(0);
}

/**
 * Free Linear Memory, leaving stack in place
 * @param sandbox
 **/
void
sandbox_free_linear_memory(struct sandbox *sandbox)
{
	int rc = munmap(sandbox->linear_memory_start, SBOX_MAX_MEM + PAGE_SIZE);
	if (rc == -1) {
		perror("sandbox_free_linear_memory - munmap failed\n");
		assert(0);
	}
}

/**
 * Free stack and heap resources.. also any I/O handles.
 * @param sandbox
 **/
void
sandbox_free(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	if (sandbox->state != SANDBOX_ERROR && sandbox->state != SANDBOX_COMPLETE) {
		printf("Unexpectedly attempted to free a sandbox in a %s state\n", sandbox_get_state(sandbox));
		assert(0);
	};

	char *error_message = NULL;
	int   rc;

	module_release(sandbox->module);

	void * stkaddr = sandbox->stack_start;
	size_t stksz   = sandbox->stack_size;


	// Free Sandbox Stack
	errno = 0;
	rc    = munmap(stkaddr, stksz);
	if (rc == -1) {
		error_message = "Failed to unmap stack";
		goto err_free_stack_failed;
	};


	// Free Sandbox Linear Address Space
	// struct sandbox | HTTP Buffer | 4GB of Wasm Linear Memory | Guard Page
	// sandbox_size includes the struct and HTTP buffer
	size_t sandbox_address_space_size = sandbox->sandbox_size + sandbox->linear_memory_max_size
	                                    + /* guard page */ PAGE_SIZE;

	errno = 0;
	rc    = munmap(sandbox, sandbox_address_space_size);
	if (rc == -1) {
		error_message = "Failed to unmap sandbox";
		goto err_free_sandbox_failed;
	};

done:
	return;
err_free_sandbox_failed:
err_free_stack_failed:
	// Inability to free memory is a fatal error
	perror(error_message);
	exit(EXIT_FAILURE);
err:
	goto done;
}
