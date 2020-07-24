#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <uv.h>

#include "current_sandbox.h"
#include "http_parser_settings.h"
#include "libuv_callbacks.h"
#include "panic.h"
#include "runtime.h"
#include "sandbox.h"
#include "types.h"
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
 * Run the http-parser on the sandbox's request_response_data using the configured settings global
 * @param sandbox the sandbox containing the req_resp data that we want to parse
 * @param length The size of the request_response_data that we want to parse
 * @returns 0
 *
 */
int
sandbox_parse_http_request(struct sandbox *sandbox, size_t length)
{
	assert(sandbox != NULL);
	assert(length > 0);
	/* Why is our start address sandbox->request_response_data + sandbox->request_response_data_length?
	it's like a cursor to keep track of what we've read so far */
	http_parser_execute(&sandbox->http_parser, http_parser_settings_get(),
	                    sandbox->request_response_data + sandbox->request_response_data_length, length);
	return 0;
}

/**
 * Receive and Parse the Request for the current sandbox
 * @return 1 on success, < 0 on failure.
 */
static inline int
sandbox_receive_and_parse_client_request(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	sandbox->request_response_data_length = 0;
#ifndef USE_HTTP_UVIO
	int r = 0;
	r = recv(sandbox->client_socket_descriptor, (sandbox->request_response_data), sandbox->module->max_request_size,
	         0);
	if (r < 0) {
		perror("Error reading request data from client socket");
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
	sandbox->request_length = sandbox->request_response_data_length;
	return 1;
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
	assert(response_cursor < sandbox->request_length);

	/* Move the Sandbox's Data after the HTTP Response Data */
	memmove(sandbox->request_response_data + response_cursor,
	        sandbox->request_response_data + sandbox->request_length, body_size);
	response_cursor += body_size;

	/* Capture Timekeeping data for end-to-end latency */
	uint64_t end_time      = __getcycles();
	sandbox->total_time    = end_time - sandbox->request_arrival_timestamp;
	uint64_t total_time_us = sandbox->total_time / runtime_processor_speed_MHz;

	debuglog("%s():%d, %u, %lu\n", sandbox->module->name, sandbox->module->port,
	         sandbox->module->relative_deadline_us, total_time_us);

#ifndef USE_HTTP_UVIO
	int r = send(sandbox->client_socket_descriptor, sandbox->request_response_data, response_cursor, 0);
	if (r < 0) {
		perror("send");
		return -1;
	}
	while (r < response_cursor) {
		int s = send(sandbox->client_socket_descriptor, sandbox->request_response_data + r, response_cursor - r,
		             0);
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
	uv_buf_t bufv = uv_buf_init(sandbox->request_response_data, response_cursor);
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

	/* Set the sandbox as the data the http-parser has access to */
	sandbox->http_parser.data = sandbox;

#ifdef USE_HTTP_UVIO

	/* Initialize libuv TCP stream */
	int r = uv_tcp_init(worker_thread_get_libuv_handle(), (uv_tcp_t *)&sandbox->client_libuv_stream);
	assert(r == 0);

	/* Set the current sandbox as the data the libuv callbacks have access to */
	sandbox->client_libuv_stream.data = sandbox;

	/* Open the libuv TCP stream */
	r = uv_tcp_open((uv_tcp_t *)&sandbox->client_libuv_stream, sandbox->client_socket_descriptor);
	assert(r == 0);
#endif
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

char *
sandbox_state_stringify(sandbox_state_t state)
{
	switch (state) {
	case SANDBOX_UNINITIALIZED:
		return "Uninitialized";
	case SANDBOX_ALLOCATED:
		return "Allocated";
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
		/* Crash, as this should be exclusive */
		assert(0);
	}
}

/**
 * Sandbox execution logic
 * Handles setup, request parsing, WebAssembly initialization, function execution, response building and sending, and
 * cleanup
 */
void
current_sandbox_main(void)
{
	struct sandbox *sandbox = current_sandbox_get();
	assert(sandbox != NULL);
	assert(sandbox->state == SANDBOX_RUNNABLE);

	char *error_message = "";

	assert(!software_interrupt_is_enabled());
	arch_context_init(&sandbox->ctxt, 0, 0);
	software_interrupt_enable();

	sandbox_initialize_io_handles_and_file_descriptors(sandbox);

	sandbox_open_http(sandbox);

	/* Parse the request. 1 = Success */
	int rc = sandbox_receive_and_parse_client_request(sandbox);
	if (rc != 1) {
		error_message = "Unable to receive and parse client request\n";
		goto err;
	};

	/* Initialize the module */
	struct module *current_module = sandbox_get_module(sandbox);
	int            argument_count = module_get_argument_count(current_module);
	// alloc_linear_memory();
	module_initialize_globals(current_module);
	module_initialize_memory(current_module);

	/* Copy the arguments into the WebAssembly sandbox */
	sandbox_setup_arguments(sandbox);

	/* Executing the function */
	sandbox->return_value = module_main(current_module, argument_count, sandbox->arguments_offset);

	/* Retrieve the result, construct the HTTP response, and send to client */
	rc = sandbox_build_and_send_client_response(sandbox);
	if (rc == -1) {
		error_message = "Unable to build and send client response\n";
		goto err;
	};

done:
	/* Cleanup connection and exit sandbox */
	sandbox_close_http(sandbox);
	worker_thread_on_sandbox_exit(sandbox);

	/* This assert prevents a segfault discussed in
	 * https://github.com/phanikishoreg/awsm-Serverless-Framework/issues/66
	 */
	assert(0);
err:
	fprintf(stderr, "%s", error_message);
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
	uint64_t        linear_memory_max_size = (uint64_t)SBOX_MAX_MEM;
	struct sandbox *sandbox                = NULL;
	unsigned long   sandbox_size           = sizeof(struct sandbox) + module->max_request_or_response_size;

	/* Control information should be page-aligned
	        TODO: Should I use round_up_to_page when setting sandbox_page? */
	assert(round_up_to_page(sandbox_size) == sandbox_size);

	/* At an address of the system's choosing, allocate the memory, marking it as inaccessible */
	errno      = 0;
	void *addr = mmap(NULL, sandbox_size + linear_memory_max_size + /* guard page */ PAGE_SIZE, PROT_NONE,
	                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		error_message = "sandbox_allocate_memory - memory allocation failed";
		goto alloc_failed;
	}
	sandbox = (struct sandbox *)addr;

	/* Set the struct sandbox, HTTP Req/Resp buffer, and the initial Wasm Pages as read/write */
	errno         = 0;
	void *addr_rw = mmap(addr, sandbox_size + linear_memory_size, PROT_READ | PROT_WRITE,
	                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (addr_rw == MAP_FAILED) {
		error_message = "set to r/w";
		goto set_rw_failed;
	}

	/* Populate Sandbox members */
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

	/* Allocate Sandbox control structures, buffers, and linear memory in a 4GB address space */
	sandbox = sandbox_allocate_memory(sandbox_request->module);
	if (!sandbox) {
		error_message = "failed to allocate sandbox heap and linear memory";
		goto err_memory_allocation_failed;
	}

	/* Set state to initializing */
	sandbox->state = SANDBOX_UNINITIALIZED;

	/* Allocate the Stack */
	if (sandbox_allocate_stack(sandbox) < 0) {
		error_message = "failed to allocate sandbox heap and linear memory";
		goto err_stack_allocation_failed;
	}

	/* Copy the socket descriptor, address, and arguments of the client invocation */
	sandbox->absolute_deadline         = sandbox_request->absolute_deadline;
	sandbox->arguments                 = (void *)sandbox_request->arguments;
	sandbox->client_socket_descriptor  = sandbox_request->socket_descriptor;
	sandbox->request_arrival_timestamp = sandbox_request->request_arrival_timestamp;

	/* Initialize the sandbox's context, stack, and instruction pointer */
	arch_context_init(&sandbox->ctxt, (reg_t)current_sandbox_main,
	                  (reg_t)(sandbox->stack_start + sandbox->stack_size));

	/* TODO: What does it mean if there isn't a socket_address? Shouldn't this be a hard requirement?
	        It seems that only the socket descriptor is used to send response */
	const struct sockaddr *socket_address = sandbox_request->socket_address;
	if (socket_address) memcpy(&sandbox->client_address, socket_address, sizeof(struct sockaddr));

	/* Initialize file descriptors to -1 */
	for (int i = 0; i < SANDBOX_MAX_IO_HANDLE_COUNT; i++) sandbox->io_handles[i].file_descriptor = -1;

	/* Initialize Parsec control structures (used by Completion Queue) */
	ps_list_init_d(sandbox);

	free(sandbox_request);
done:
	return sandbox;
err_stack_allocation_failed:
	sandbox_free(sandbox);
err_memory_allocation_failed:
err:
	perror(error_message);
	sandbox = NULL;
	goto done;
}

/**
 * Free Linear Memory, leaving stack in place
 * @param sandbox
 **/
void
sandbox_free_linear_memory(struct sandbox *sandbox)
{
	int rc = munmap(sandbox->linear_memory_start, SBOX_MAX_MEM + PAGE_SIZE);
	if (rc == -1) panic("sandbox_free_linear_memory - munmap failed\n");
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
	assert(sandbox->state == SANDBOX_UNINITIALIZED || sandbox->state == SANDBOX_RETURNED);

	char *error_message = NULL;
	int   rc;

	module_release(sandbox->module);

	void * stkaddr = sandbox->stack_start;
	size_t stksz   = sandbox->stack_size;


	/* Free Sandbox Stack */
	errno = 0;
	rc    = munmap(stkaddr, stksz);
	if (rc == -1) {
		fprintf(stderr, "Thread %lu | Failed to unmap stack %p\n", pthread_self(), sandbox);
		goto err_free_stack_failed;
	};


	/* Free Sandbox Linear Address Space
	struct sandbox | HTTP Buffer | 4GB of Wasm Linear Memory | Guard Page
	sandbox_size includes the struct and HTTP buffer */
	size_t sandbox_address_space_size = sandbox->sandbox_size + sandbox->linear_memory_max_size
	                                    + /* guard page */ PAGE_SIZE;

	errno = 0;
	rc    = munmap(sandbox, sandbox_address_space_size);
	if (rc == -1) {
		fprintf(stderr, "Thread %lu | Failed to unmap sanbox %p\n", pthread_self(), sandbox);
		goto err_free_sandbox_failed;
	};

done:
	return;
err_free_sandbox_failed:
err_free_stack_failed:
err:
	/* Errors freeing memory is a fatal error */
	panic("Thread %lu | Failed to free sandbox %p\n", pthread_self(), sandbox);
}
