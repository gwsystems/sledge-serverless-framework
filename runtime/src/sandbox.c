#include <assert.h>
#include <runtime.h>
#include <sandbox.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>
#include <uv.h>
#include <libuv_callbacks.h>
#include <util.h>
#include <current_sandbox.h>

/**
 * Takes the arguments from the sandbox struct and writes them into the WebAssembly linear memory
 * TODO: why do we have to pass argument count explicitly? Can't we just get this off the sandbox?
 * @param argument_count
 **/
static inline void
current_sandbox__setup_arguments(i32 argument_count)
{
	struct sandbox *curr = current_sandbox__get();
	char *          arguments = current_sandbox__get_arguments();

	// whatever gregor has, to be able to pass arguments to a module!
	curr->arguments_offset = sandbox_lmbound;
	assert(sandbox_lmbase == curr->linear_memory_start);
	expand_memory();

	i32 *array_ptr  = get_memory_ptr_void(curr->arguments_offset, argument_count * sizeof(i32));
	i32  string_off = curr->arguments_offset + (argument_count * sizeof(i32));

	for (int i = 0; i < argument_count; i++) {
		char * arg    = arguments + (i * MOD_ARG_MAX_SZ);
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
 * Globals: runtime__http_parser_settings
 **/
int
sandbox__parse_http_request(struct sandbox *sandbox, size_t length)
{
	// Why is our start address sandbox->request_response_data + sandbox->request_response_data_length?
	// it's like a cursor to keep track of what we've read so far
	http_parser_execute(&sandbox->http_parser, &runtime__http_parser_settings, sandbox->request_response_data + sandbox->request_response_data_length, length);
	return 0;
}


/**
 * Receive and Parse the Request for the current sandbox
 * @return 1 on success, 0 if no context, < 0 on failure. 
 **/
static inline int
current_sandbox__receive_and_parse_client_request(void)
{
	struct sandbox *curr = current_sandbox__get();
	curr->request_response_data_length    = 0;
#ifndef USE_HTTP_UVIO
	int r = 0;
	r     = recv(curr->client_socket_descriptor, (curr->request_response_data), curr->module->max_request_size, 0);
	if (r <= 0) {
		if (r < 0) perror("recv1");
		return r;
	}
	while (r > 0) {
		if (current_sandbox__parse_http_request(r) != 0) return -1;
		curr->request_response_data_length += r;
		struct http_request *rh = &curr->http_request;
		if (rh->message_end) break;

		r = recv(curr->client_socket_descriptor, (curr->request_response_data + curr->request_response_data_length),
		         curr->module->max_request_size - curr->request_response_data_length, 0);
		if (r < 0) {
			perror("recv2");
			return r;
		}
	}
#else
	int r = uv_read_start((uv_stream_t *)&curr->client_libuv_stream, libuv_callbacks__on_allocate_setup_request_response_data, libuv_callbacks__on_read_parse_http_request);
	sandbox_block_http();
	if (curr->request_response_data_length == 0) return 0;
#endif
	return 1;
}

/**
 * Sends Response Back to Client
 * @return RC. -1 on Failure
 **/
static inline int
current_sandbox__build_and_send_client_response(void)
{
	int             sndsz       = 0;
	struct sandbox *curr        = current_sandbox__get();
	int             response_header_length = strlen(HTTP_RESP_200OK) + strlen(HTTP_RESP_CONTTYPE) + strlen(HTTP_RESP_CONTLEN);
	int             body_length = curr->request_response_data_length - response_header_length;

	memset(curr->request_response_data, 0,
	       strlen(HTTP_RESP_200OK) + strlen(HTTP_RESP_CONTTYPE) + strlen(HTTP_RESP_CONTLEN));
	strncpy(curr->request_response_data, HTTP_RESP_200OK, strlen(HTTP_RESP_200OK));
	sndsz += strlen(HTTP_RESP_200OK);

	if (body_length == 0) goto done;
	strncpy(curr->request_response_data + sndsz, HTTP_RESP_CONTTYPE, strlen(HTTP_RESP_CONTTYPE));
	if (strlen(curr->module->response_content_type) <= 0) {
		strncpy(curr->request_response_data + sndsz + strlen("Content-type: "), HTTP_RESP_CONTTYPE_PLAIN,
		        strlen(HTTP_RESP_CONTTYPE_PLAIN));
	} else {
		strncpy(curr->request_response_data + sndsz + strlen("Content-type: "), curr->module->response_content_type,
		        strlen(curr->module->response_content_type));
	}
	sndsz += strlen(HTTP_RESP_CONTTYPE);
	char len[10] = { 0 };
	sprintf(len, "%d", body_length);
	strncpy(curr->request_response_data + sndsz, HTTP_RESP_CONTLEN, strlen(HTTP_RESP_CONTLEN));
	strncpy(curr->request_response_data + sndsz + strlen("Content-length: "), len, strlen(len));
	sndsz += strlen(HTTP_RESP_CONTLEN);
	sndsz += body_length;

done:
	assert(sndsz == curr->request_response_data_length);
	// Get End Timestamp
	curr->total_time = util__rdtsc() - curr->start_time;
	printf("Function returned in %lu cycles\n", curr->total_time);

#ifndef USE_HTTP_UVIO
	int r = send(curr->client_socket_descriptor, curr->request_response_data, sndsz, 0);
	if (r < 0) {
		perror("send");
		return -1;
	}
	while (r < sndsz) {
		int s = send(curr->client_socket_descriptor, curr->request_response_data + r, sndsz - r, 0);
		if (s < 0) {
			perror("send");
			return -1;
		}
		r += s;
	}
#else
	uv_write_t req = {
		.data = curr,
	};
	uv_buf_t bufv = uv_buf_init(curr->request_response_data, sndsz);
	int      r    = uv_write(&req, (uv_stream_t *)&curr->client_libuv_stream, &bufv, 1, libuv_callbacks__on_write_wakeup_sandbox);
	sandbox_block_http();
#endif
	return 0;
}

/**
 * Sandbox execution logic
 * Handles setup, request parsing, WebAssembly initialization, function execution, response building and sending, and cleanup
 **/
void
sandbox_main(void)
{
	struct sandbox *current_sandbox = current_sandbox__get();
	// FIXME: is this right? this is the first time this sandbox is running.. so it wont
	//        return to switch_to_sandbox() api..
	//        we'd potentially do what we'd in switch_to_sandbox() api here for cleanup..
	if (!softint__is_enabled()) {
		arch_context_init(&current_sandbox->ctxt, 0, 0);
		next_context = NULL;
		softint__enable();
	}
	struct module *current_module       = sandbox__get_module(current_sandbox);
	int            argument_count = module__get_argument_count(current_module);
	// for stdio

	// Try to initialize file descriptors 0, 1, and 2 as io handles 0, 1, 2
	// We need to check that we get what we expect, as these IO handles may theoretically have been taken
	// TODO: why do the file descriptors have to match the io handles?
	int f = current_sandbox__initialize_io_handle_and_set_file_descriptor(0);
	assert(f == 0);
	f = current_sandbox__initialize_io_handle_and_set_file_descriptor(1);
	assert(f == 1);
	f = current_sandbox__initialize_io_handle_and_set_file_descriptor(2);
	assert(f == 2);

	// Initialize the HTTP-Parser for a request
	http_parser_init(&current_sandbox->http_parser, HTTP_REQUEST);

	// Set the current_sandbox as the data the http-parser has access to
	current_sandbox->http_parser.data = current_sandbox;
	
	// NOTE: if more headers, do offset by that!
	int response_header_length = strlen(HTTP_RESP_200OK) + strlen(HTTP_RESP_CONTTYPE) + strlen(HTTP_RESP_CONTLEN);

#ifdef USE_HTTP_UVIO

	// Initialize libuv TCP stream
	int r = uv_tcp_init(get_thread_libuv_handle(), (uv_tcp_t *)&current_sandbox->client_libuv_stream);
	assert(r == 0);

	// Set the current sandbox as the data the libuv callbacks have access to
	current_sandbox->client_libuv_stream.data = current_sandbox;

	// Open the libuv TCP stream
	r              = uv_tcp_open((uv_tcp_t *)&current_sandbox->client_libuv_stream, current_sandbox->client_socket_descriptor);
	assert(r == 0);
#endif

	// If the HTTP Request returns 1, we've successfully received and parsed the HTTP request, so execute it!
	if (current_sandbox__receive_and_parse_client_request() > 0) {

		//
		current_sandbox->request_response_data_length = response_header_length;

		// Allocate the WebAssembly Sandbox
		alloc_linear_memory();
		module__initialize_globals(current_module);
		module__initialize_memory(current_module);

		// Copy the arguments into the WebAssembly sandbox
		current_sandbox__setup_arguments(argument_count);

		// Executing the function within the WebAssembly sandbox
		current_sandbox->return_value = module__main(current_module, argument_count, current_sandbox->arguments_offset);

		// Retrieve the result from the WebAssembly sandbox, construct the HTTP response, and send to client
		current_sandbox__build_and_send_client_response();
	}

	// Cleanup connection and exit sandbox

#ifdef USE_HTTP_UVIO
	uv_close((uv_handle_t *)&current_sandbox->client_libuv_stream, libuv_callbacks__on_close_wakeup_sakebox);
	sandbox_block_http();
#else
	close(current_sandbox->client_socket_descriptor);
#endif
	current_sandbox__exit();
}

/**
 * Allocates the memory for a sandbox to run a module
 * @param module the module that we want to run
 * @returns the resulting sandbox or NULL if mmap failed
 **/
static inline struct sandbox *
sandbox__allocate_memory(struct module *module)
{
	unsigned long memory_size = SBOX_MAX_MEM; // 4GB

	// Why do we add max_request_or_response_size?
	unsigned long sandbox_size       = sizeof(struct sandbox) + module->max_request_or_response_size;
	unsigned long linear_memory_size = WASM_PAGE_SIZE * WASM_START_PAGES;

	if (linear_memory_size + sandbox_size > memory_size) return NULL;
	assert(round_up_to_page(sandbox_size) == sandbox_size);

	// What does mmap do exactly with file_descriptor -1?
	void *addr = mmap(NULL, sandbox_size + memory_size + /* guard page */ PAGE_SIZE, PROT_NONE,
	                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) return NULL;

	void *addr_rw = mmap(addr, sandbox_size + linear_memory_size, PROT_READ | PROT_WRITE,
	                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (addr_rw == MAP_FAILED) {
		munmap(addr, memory_size + PAGE_SIZE);
		return NULL;
	}

	struct sandbox *sandbox = (struct sandbox *)addr;
	// can it include sandbox as well?
	sandbox->linear_memory_start = (char *)addr + sandbox_size;
	sandbox->linear_memory_size  = linear_memory_size;
	sandbox->module       = module;
	sandbox->sandbox_size = sandbox_size;
	module__acquire(module);

	return sandbox;
}

struct sandbox *
sandbox__allocate(struct module *module, char *arguments, int socket_descriptor, const struct sockaddr *socket_address, u64 start_time)
{
	if (!module__is_valid(module)) return NULL;

	// FIXME: don't use malloc. huge security problem!
	// perhaps, main should be in its own sandbox, when it is not running any sandbox.
	struct sandbox *sandbox = (struct sandbox *)sandbox__allocate_memory(module);
	if (!sandbox) return NULL;

	// Assign the start time from the request
	sandbox->start_time = start_time;

	// actual module instantiation!
	sandbox->arguments        = (void *)arguments;
	sandbox->stack_size  = module->stack_size;
	sandbox->stack_start = mmap(NULL, sandbox->stack_size, PROT_READ | PROT_WRITE,
	                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN, -1, 0);
	if (sandbox->stack_start == MAP_FAILED) {
		perror("mmap");
		assert(0);
	}
	sandbox->client_socket_descriptor = socket_descriptor;
	if (socket_address) memcpy(&sandbox->client_address, socket_address, sizeof(struct sockaddr));
	for (int i = 0; i < SBOX_MAX_OPEN; i++) sandbox->handles[i].file_descriptor = -1;
	ps_list_init_d(sandbox);

	// Setup the sandbox's context, stack, and instruction pointer
	arch_context_init(&sandbox->ctxt, (reg_t)sandbox_main, (reg_t)(sandbox->stack_start + sandbox->stack_size));
	return sandbox;
}

void
sandbox__free(struct sandbox *sandbox)
{
	// you have to context switch away to free a sandbox.
	if (!sandbox || sandbox == current_sandbox__get()) return;

	// again sandbox should be done and waiting for the parent.
	if (sandbox->state != RETURNED) return;

	int sz = sizeof(struct sandbox);

	sz += sandbox->module->max_request_or_response_size;
	module__release(sandbox->module);

	void * stkaddr = sandbox->stack_start;
	size_t stksz   = sandbox->stack_size;

	// depending on the memory type
	// free_linear_memory(sandbox->linear_memory_start, sandbox->linear_memory_size, sandbox->linear_memory_max_size);

	int ret;
	// mmaped memory includes sandbox structure in there.
	ret = munmap(sandbox, sz);
	if (ret) perror("munmap sandbox");

	// remove stack!
	// for some reason, removing stack seem to cause crash in some cases.
	ret = munmap(stkaddr, stksz);
	if (ret) perror("munmap stack");
}
