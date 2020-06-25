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
		if (r < 0) perror("Error reading request data from client socket");
		if (r == 0) perror("No data to reach from client socket");
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
	if (sandbox->request_response_data_length == 0) {
		perror("request_response_data_length was unexpectedly 0");
		return 0
	};
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
sandbox_state_stringify(sandbox_state_t state)
{
	switch (state) {
	case SANDBOX_UNINITIALIZED:
		return "Uninitialized";
	case SANDBOX_SET_AS_INITIALIZED:
		return "Set As Initialized";
	case SANDBOX_INITIALIZED:
		return "Initialized";
	case SANDBOX_SET_AS_RUNNABLE:
		return "Set As Runnable";
	case SANDBOX_RUNNABLE:
		return "Runnable";
	case SANDBOX_SET_AS_RUNNING:
		return "Set As Running";
	case SANDBOX_RUNNING:
		return "Running";
	case SANDBOX_SET_AS_PREEMPTED:
		return "Set As Preempted";
	case SANDBOX_PREEMPTED:
		return "Preempted";
	case SANDBOX_SET_AS_BLOCKED:
		return "Set As Blocked";
	case SANDBOX_BLOCKED:
		return "Blocked";
	case SANDBOX_SET_AS_RETURNED:
		return "Set As Returned";
	case SANDBOX_RETURNED:
		return "Returned";
	case SANDBOX_SET_AS_COMPLETE:
		return "Set As Complete";
	case SANDBOX_COMPLETE:
		return "Complete";
	case SANDBOX_SET_AS_ERROR:
		return "Set As Error";
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
 * cleanup
 **/
void
current_sandbox_main(void)
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	if (sandbox->state != SANDBOX_RUNNING) {
		printf("Expected Sandbox to be in Running state, but found %s\n",
		       sandbox_state_stringify(sandbox->state));
		assert(0);
	};

	char *error_message = "";

	assert(!software_interrupt_is_enabled());
	arch_context_init(&sandbox->ctxt, 0, 0);
	worker_thread_is_switching_context = false;
	software_interrupt_enable();

	sandbox_initialize_io_handles_and_file_descriptors(sandbox);

	sandbox_open_http(sandbox);

	// Parse the request. 1 = Success
	errno  = 0;
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
	errno = 0;
	rc    = sandbox_build_and_send_client_response(sandbox);
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
	       sandbox->module->name, sandbox->module->port, sandbox_state_stringify(sandbox->state),
	       sandbox->module->relative_deadline_us, total_time_us, queued_us, initializing_us, runnable_us,
	       running_us, blocked_us, returned_us);
}

/**
 * Transitions a sandbox to the SANDBOX_INITIALIZED state. Because this is the initial state of a new sandbox, we have
 * to assume that sandbox->state is garbage.
 *
 * TODO: Consider zeroing out allocation of the sandbox struct to be able to assert that we are only calling this on a
 * clean allocation. Additionally, we might want to shift the sandbox states up, so zero is distinct from
 * SANDBOX_INITIALIZED
 * @param sandbox
 **/
void
sandbox_set_as_initialized(sandbox_t *sandbox, sandbox_request_t *sandbox_request, uint64_t allocation_timestamp)
{
	assert(sandbox != NULL);
	assert(sandbox_request != NULL);
	assert(sandbox_request->arguments != NULL);
	assert(sandbox_request->request_timestamp > 0);
	assert(sandbox_request->socket_address != NULL);
	assert(sandbox_request->socket_descriptor > 0);
	assert(allocation_timestamp > 0);
	assert(sandbox->state == SANDBOX_UNINITIALIZED);
	printf("Thread %lu | Sandbox %lu | Uninitialized => Initialized\n", pthread_self(), allocation_timestamp);

	sandbox->request_timestamp           = sandbox_request->request_timestamp;
	sandbox->allocation_timestamp        = allocation_timestamp;
	sandbox->response_timestamp          = 0;
	sandbox->completion_timestamp        = 0;
	sandbox->last_state_change_timestamp = allocation_timestamp;
	sandbox_state_t last_state           = sandbox->state;
	sandbox->state                       = SANDBOX_SET_AS_INITIALIZED;

	// Initialize the sandbox's context, stack, and instruction pointer
	arch_context_init(&sandbox->ctxt, (reg_t)current_sandbox_main,
	                  (reg_t)(sandbox->stack_start + sandbox->stack_size));

	// Initialize file descriptors to -1
	for (int i = 0; i < SANDBOX_MAX_IO_HANDLE_COUNT; i++) sandbox->io_handles[i].file_descriptor = -1;


	// Initialize Parsec control structures (used by Completion Queue)
	ps_list_init_d(sandbox);


	// Copy the socket descriptor, address, and arguments of the client invocation
	sandbox->absolute_deadline = sandbox_request->absolute_deadline;
	sandbox->total_time        = 0;

	sandbox->arguments                = (void *)sandbox_request->arguments;
	sandbox->client_socket_descriptor = sandbox_request->socket_descriptor;
	memcpy(&sandbox->client_address, sandbox_request->socket_address, sizeof(struct sockaddr));


	sandbox->initializing_duration = 0;
	sandbox->runnable_duration     = 0;
	sandbox->preempted_duration    = 0;
	sandbox->running_duration      = 0;
	sandbox->blocked_duration      = 0;
	sandbox->returned_duration     = 0;


	sandbox->state = SANDBOX_INITIALIZED;
}

static void
dump_regs(const mcontext_t *ctxt)
{
	printf("REG_R8: %lld\n", ctxt->gregs[0]);
	printf("REG_R9: %lld\n", ctxt->gregs[1]);
	printf("REG_R10: %lld\n", ctxt->gregs[2]);
	printf("REG_R11: %lld\n", ctxt->gregs[3]);
	printf("REG_R12: %lld\n", ctxt->gregs[4]);
	printf("REG_R13: %lld\n", ctxt->gregs[5]);
	printf("REG_R14: %lld\n", ctxt->gregs[6]);
	printf("REG_R15: %lld\n", ctxt->gregs[7]);
	printf("REG_RDI: %lld\n", ctxt->gregs[8]);
	printf("REG_RSI: %lld\n", ctxt->gregs[9]);
	printf("REG_RBP: %lld\n", ctxt->gregs[10]);
	printf("REG_RBX: %lld\n", ctxt->gregs[11]);
	printf("REG_RDX: %lld\n", ctxt->gregs[12]);
	printf("REG_RAX: %lld\n", ctxt->gregs[13]);
	printf("REG_RCX: %lld\n", ctxt->gregs[14]);
	printf("REG_RSP: %lld\n", ctxt->gregs[15]);
	printf("REG_RIP: %lld\n", ctxt->gregs[16]);
	printf("REG_EFL: %lld\n", ctxt->gregs[17]);
	printf("REG_CSGSFS: %lld\n", ctxt->gregs[18]);
	printf("REG_ERR: %lld\n", ctxt->gregs[19]);
	printf("REG_TRAPNO: %lld\n", ctxt->gregs[20]);
	printf("REG_OLDMASK: %lld\n", ctxt->gregs[21]);
	printf("REG_CR2: %lld\n", ctxt->gregs[22]);
	// Ignoring FPU
}

/**
 * Transitions a sandbox to the SANDBOX_RUNNABLE state.
 *
 * This occurs in the following scenarios:
 * - A sandbox in the SANDBOX_INITIALIZED state completes initialization and is ready to be run
 * - A sandbox in the SANDBOX_BLOCKED state completes what was blocking it and is ready to be run
 * - A sandbox in the SANDBOX_RUNNING state is preempted before competion and is ready to be run
 *
 * @param sandbox
 **/
void
sandbox_set_as_runnable(sandbox_t *sandbox)
{
	assert(sandbox);
	assert(sandbox->last_state_change_timestamp > 0);
	uint64_t        now                    = __getcycles();
	uint64_t        duration_of_last_state = now - sandbox->last_state_change_timestamp;
	sandbox_state_t last_state             = sandbox->state;
	sandbox->state                         = SANDBOX_SET_AS_RUNNABLE;
	printf("Thread %lu | Sandbox %lu | %s => Runnable\n", pthread_self(), sandbox->allocation_timestamp,
	       sandbox_state_stringify(last_state));

	switch (last_state) {
	case SANDBOX_INITIALIZED: {
		sandbox->initializing_duration += duration_of_last_state;
		sandbox_run_queue_add(sandbox);
		break;
	}
	case SANDBOX_BLOCKED: {
		sandbox->blocked_duration += duration_of_last_state;
		sandbox_run_queue_add(sandbox);
		break;
	}
	default: {
		printf("Thread %lu | Sandbox %lu | Illegal transition from %s to Runnable\n", pthread_self(),
		       sandbox->allocation_timestamp, sandbox_state_stringify(last_state));
		assert(0);
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RUNNABLE;
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
 * @param active_context - the worker thread context that is going to execute this sandbox. Only provided
 * when performing a full mcontext restore
 **/
void
sandbox_set_as_running(sandbox_t *sandbox)
{
	assert(sandbox);
	uint64_t        now                      = __getcycles();
	uint64_t        duration_of_last_state   = now - sandbox->last_state_change_timestamp;
	sandbox_state_t last_state               = sandbox->state;
	bool            should_enable_interrupts = false;

	sandbox->state = SANDBOX_SET_AS_RUNNING;
	printf("Thread %lu | Sandbox %lu | %s => Running\n", pthread_self(), sandbox->allocation_timestamp,
	       sandbox_state_stringify(last_state));

	switch (last_state) {
	case SANDBOX_RUNNABLE: {
		sandbox->runnable_duration += duration_of_last_state;
		current_sandbox_set(sandbox);
		break;
	}
	case SANDBOX_PREEMPTED: {
		// is active_context an invarient here?
		sandbox->preempted_duration += duration_of_last_state;
		current_sandbox_set(sandbox);
		break;
	}
	default: {
		printf("Thread %lu | Sandbox %lu | Illegal transition from %s to Running\n", pthread_self(),
		       sandbox->allocation_timestamp, sandbox_state_stringify(last_state));
		assert(0);
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_RUNNING;

	if (should_enable_interrupts) software_interrupt_enable();
}

void
sandbox_set_as_preempted(sandbox_t *sandbox)
{
	assert(sandbox);
	uint64_t        now                    = __getcycles();
	uint64_t        duration_of_last_state = now - sandbox->last_state_change_timestamp;
	sandbox_state_t last_state             = sandbox->state;
	sandbox->state                         = SANDBOX_SET_AS_PREEMPTED;
	printf("Thread %lu | Sandbox %lu | %s => Preempted\n", pthread_self(), sandbox->allocation_timestamp,
	       sandbox_state_stringify(last_state));

	switch (last_state) {
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		break;
	}
	default: {
		printf("Thread %lu | Sandbox %lu | Illegal transition from %s to Preempted\n", pthread_self(),
		       sandbox->allocation_timestamp, sandbox_state_stringify(last_state));
		assert(0);
	}
	}

	sandbox->last_state_change_timestamp = now;
	sandbox->state                       = SANDBOX_PREEMPTED;
}

void
sandbox_set_as_blocked(sandbox_t *sandbox)
{
	assert(sandbox);
	uint64_t        now                    = __getcycles();
	uint64_t        duration_of_last_state = now - sandbox->last_state_change_timestamp;
	sandbox_state_t last_state             = sandbox->state;
	sandbox->state                         = SANDBOX_SET_AS_BLOCKED;
	printf("Thread %lu | Sandbox %lu | %s => Blocked\n", pthread_self(), sandbox->allocation_timestamp,
	       sandbox_state_stringify(last_state));

	switch (last_state) {
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		sandbox_run_queue_delete(sandbox);
		break;
	}
	default: {
		printf("Thread %lu | Sandbox %lu | Illegal transition from %s to Blocked\n", pthread_self(),
		       sandbox->allocation_timestamp, sandbox_state_stringify(last_state));
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
	uint64_t        now                    = __getcycles();
	uint64_t        duration_of_last_state = now - sandbox->last_state_change_timestamp;
	sandbox_state_t last_state             = sandbox->state;
	sandbox->state                         = SANDBOX_SET_AS_RETURNED;
	printf("Thread %lu | Sandbox %lu | %s => Returned\n", pthread_self(), sandbox->allocation_timestamp,
	       sandbox_state_stringify(last_state));

	switch (last_state) {
	case SANDBOX_RUNNING: {
		sandbox->response_timestamp = now;
		sandbox->total_time         = now - sandbox->request_timestamp;
		sandbox->running_duration += duration_of_last_state;
		sandbox_run_queue_delete(sandbox);
		sandbox_free_linear_memory(sandbox);
		break;
	}
	default: {
		printf("Thread %lu | Sandbox %lu | Illegal transition from %s to Returned\n", pthread_self(),
		       sandbox->allocation_timestamp, sandbox_state_stringify(last_state));
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
	uint64_t        now                    = __getcycles();
	uint64_t        duration_of_last_state = now - sandbox->last_state_change_timestamp;
	sandbox_state_t last_state             = sandbox->state;
	sandbox->state                         = SANDBOX_SET_AS_ERROR;
	bool should_add_to_completion_queue    = false;
	printf("Thread %lu | Sandbox %lu | %s => Error\n", pthread_self(), sandbox->allocation_timestamp,
	       sandbox_state_stringify(last_state));

	switch (last_state) {
	case SANDBOX_SET_AS_INITIALIZED:
		// Note: technically, this is a degenerate sandbox that we hand
		sandbox->initializing_duration += duration_of_last_state;
		sandbox_free_linear_memory(sandbox);
		should_add_to_completion_queue = true;
		break;
	case SANDBOX_RUNNING: {
		sandbox->running_duration += duration_of_last_state;
		sandbox_run_queue_delete(sandbox);
		sandbox_free_linear_memory(sandbox);
		should_add_to_completion_queue = true;
		break;
	}
	default: {
		printf("Thread %lu | Sandbox %lu | Illegal transition from %s to Error\n", pthread_self(),
		       sandbox->allocation_timestamp, sandbox_state_stringify(last_state));
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
	uint64_t        now                    = __getcycles();
	uint64_t        duration_of_last_state = now - sandbox->last_state_change_timestamp;
	sandbox_state_t last_state             = sandbox->state;
	sandbox->state                         = SANDBOX_SET_AS_COMPLETE;
	bool should_add_to_completion_queue    = false;
	printf("Thread %lu | Sandbox %lu | %s => Complete\n", pthread_self(), sandbox->allocation_timestamp,
	       sandbox_state_stringify(last_state));

	switch (last_state) {
	case SANDBOX_RETURNED: {
		sandbox->completion_timestamp = now;
		sandbox->returned_duration += duration_of_last_state;
		should_add_to_completion_queue = true;
		break;
	}
	default: {
		printf("Thread %lu | Sandbox %lu | Illegal transition from %s to Error\n", pthread_self(),
		       sandbox->allocation_timestamp, sandbox_state_stringify(last_state));
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
	if (!sandbox) {
		error_message = "failed to allocate sandbox heap and linear memory";
		goto err_memory_allocation_failed;
	};

	rc = sandbox_allocate_stack(sandbox);
	if (rc != 0) {
		error_message = "failed to allocate sandbox stack";
		goto err_stack_allocation_failed;
	};

	sandbox_set_as_initialized(sandbox, sandbox_request, now);

done:
	return sandbox;
err_stack_allocation_failed:
	// This is a degenerate sandbox that never successfully completed initialization, so we need to hand jam some
	// things to be able to cleanly transition to ERROR state
	sandbox->state                       = SANDBOX_SET_AS_INITIALIZED;
	sandbox->last_state_change_timestamp = now;
	ps_list_init_d(sandbox);
	sandbox_set_as_error(sandbox);
err_memory_allocation_failed:
err:
	perror(error_message);
	sandbox = NULL;
	goto done;
	// assert(0);
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
		printf("Unexpectedly attempted to free a sandbox in a %s state\n",
		       sandbox_state_stringify(sandbox->state));
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
