#include <assert.h>
#include <dlfcn.h>
#include <jsmn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "debuglog.h"
#include "http.h"
#include "likely.h"
#include "listener_thread.h"
#include "module.h"
#include "module_database.h"
#include "panic.h"
#include "runtime.h"
#include "scheduler.h"

const int JSON_MAX_ELEMENT_COUNT = 16;
const int JSON_MAX_ELEMENT_SIZE  = 1024;

/*************************
 * Private Static Inline *
 ************************/

/**
 * Start the module as a server listening at module->port
 * @param module
 * @returns 0 on success, -1 on error
 */
static inline int
module_listen(struct module *module)
{
	int rc;

	/* Allocate a new TCP/IP socket, setting it to be non-blocking */
	int socket_descriptor = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (unlikely(socket_descriptor < 0)) goto err_create_socket;

	/* Socket should never have returned on fd 0, 1, or 2 */
	assert(socket_descriptor != STDIN_FILENO);
	assert(socket_descriptor != STDOUT_FILENO);
	assert(socket_descriptor != STDERR_FILENO);

	/* Configure the socket to allow multiple sockets to bind to the same host and port */
	int optval = 1;
	rc         = setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	if (unlikely(rc < 0)) goto err_set_socket_option;
	optval = 1;
	rc     = setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (unlikely(rc < 0)) goto err_set_socket_option;

	/* Bind name [all addresses]:[module->port] to socket */
	module->socket_descriptor              = socket_descriptor;
	module->socket_address.sin_family      = AF_INET;
	module->socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
	module->socket_address.sin_port        = htons((unsigned short)module->port);
	rc = bind(socket_descriptor, (struct sockaddr *)&module->socket_address, sizeof(module->socket_address));
	if (unlikely(rc < 0)) goto err_bind_socket;

	/* Listen to the interface */
	rc = listen(socket_descriptor, MODULE_MAX_PENDING_CLIENT_REQUESTS);
	if (unlikely(rc < 0)) goto err_listen;


	/* Set the socket descriptor and register with our global epoll instance to monitor for incoming HTTP
	requests */
	rc = listener_thread_register_module(module);
	if (unlikely(rc < 0)) goto err_add_to_epoll;

	rc = 0;
done:
	return rc;
err_add_to_epoll:
err_listen:
err_bind_socket:
	module->socket_descriptor = -1;
err_set_socket_option:
	close(socket_descriptor);
err_create_socket:
err:
	debuglog("Socket Error: %s", strerror(errno));
	rc = -1;
	goto done;
}


/**
 * Sets the HTTP Request and Response Headers and Content type on a module
 * @param module
 * @param response_content_type
 */
static inline void
module_set_http_info(struct module *module, char response_content_type[])
{
	assert(module);
	strcpy(module->response_content_type, response_content_type);
}


/***************************************
 * Public Methods
 ***************************************/

/**
 * Module Mega Teardown Function
 * Closes the socket and dynamic library, and then frees the module
 * Returns harmlessly if there are outstanding references
 *
 * TODO: Untested Functionality. Unsure if this will work. Also, what about the module database? Do we
 * need to do any cleanup there? Issue #17
 * @param module - the module to teardown
 */
void
module_free(struct module *module)
{
	if (module == NULL) return;
	if (module->dynamic_library_handle == NULL) return;

	/* Do not free if we still have oustanding references */
	if (module->reference_count) return;


	close(module->socket_descriptor);
	dlclose(module->dynamic_library_handle);
	free(module);
}


/**
 * Module Contructor
 * Creates a new module, invokes initialize_tables to initialize the indirect table, adds it to the module DB, and
 *starts listening for HTTP Requests
 *
 * @param name
 * @param path
 * @param stack_size
 * @param max_memory
 * @param relative_deadline_us
 * @param port
 * @param request_size
 * @returns A new module or NULL in case of failure
 */

struct module *
module_new(char *name, char *path, uint32_t stack_size, uint32_t max_memory, uint32_t relative_deadline_us, int port,
           int request_size, int response_size, int admissions_percentile, uint32_t expected_execution_us)
{
	int rc = 0;

	errno                 = 0;
	struct module *module = (struct module *)malloc(sizeof(struct module));
	if (!module) {
		fprintf(stderr, "Failed to allocate module: %s\n", strerror(errno));
		goto err;
	};

	memset(module, 0, sizeof(struct module));

	atomic_init(&module->reference_count, 0);

	/* Load the dynamic library *.so file with lazy function call binding and deep binding
	 * RTLD_DEEPBIND is incompatible with certain clang sanitizers, so it might need to be temporarily disabled at
	 * times. See https://github.com/google/sanitizers/issues/611
	 */
	module->dynamic_library_handle = dlopen(path, RTLD_LAZY | RTLD_DEEPBIND);
	if (module->dynamic_library_handle == NULL) {
		fprintf(stderr, "Failed to open %s with error: %s\n", path, dlerror());
		goto dl_open_error;
	};

	/* Resolve the symbols in the dynamic library *.so file */
	module->main = (mod_main_fn_t)dlsym(module->dynamic_library_handle, MODULE_MAIN);
	if (module->main == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s in %s with error: %s\n", MODULE_MAIN, path, dlerror());
		goto dl_error;
	}

	module->initialize_globals = (mod_glb_fn_t)dlsym(module->dynamic_library_handle, MODULE_INITIALIZE_GLOBALS);
	if (module->initialize_globals == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s in %s with error: %s\n", MODULE_INITIALIZE_GLOBALS, path,
		        dlerror());
		goto dl_error;
	}

	module->initialize_memory = (mod_mem_fn_t)dlsym(module->dynamic_library_handle, MODULE_INITIALIZE_MEMORY);
	if (module->initialize_memory == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s in %s with error: %s\n", MODULE_INITIALIZE_MEMORY, path,
		        dlerror());
		goto dl_error;
	};

	module->initialize_tables = (mod_tbl_fn_t)dlsym(module->dynamic_library_handle, MODULE_INITIALIZE_TABLE);
	if (module->initialize_tables == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s in %s with error: %s\n", MODULE_INITIALIZE_TABLE, path,
		        dlerror());
		goto dl_error;
	};

	module->initialize_libc = (mod_libc_fn_t)dlsym(module->dynamic_library_handle, MODULE_INITIALIZE_LIBC);
	if (module->initialize_libc == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s in %s with error: %s\n", MODULE_INITIALIZE_LIBC, path,
		        dlerror());
		goto dl_error;
	}

	/* Set fields in the module struct */
	strncpy(module->name, name, MODULE_MAX_NAME_LENGTH);
	strncpy(module->path, path, MODULE_MAX_PATH_LENGTH);

	module->stack_size = ((uint32_t)(round_up_to_page(stack_size == 0 ? WASM_STACK_SIZE : stack_size)));
	debuglog("Stack Size: %u", module->stack_size);
	module->max_memory        = max_memory == 0 ? ((uint64_t)WASM_PAGE_SIZE * WASM_MEMORY_PAGES_MAX) : max_memory;
	module->socket_descriptor = -1;
	module->port              = port;

	/* Deadlines */
	module->relative_deadline_us = relative_deadline_us;

	/* This should have been handled when a module was loaded */
	assert(relative_deadline_us < RUNTIME_RELATIVE_DEADLINE_US_MAX);

	/* This can overflow a uint32_t, so be sure to cast appropriately */
	module->relative_deadline = (uint64_t)relative_deadline_us * runtime_processor_speed_MHz;

	/* Admissions Control */
	uint64_t expected_execution = (uint64_t)expected_execution_us * runtime_processor_speed_MHz;
	admissions_info_initialize(&module->admissions_info, admissions_percentile, expected_execution,
	                           module->relative_deadline);

	/* Request Response Buffer */
	if (request_size == 0) request_size = MODULE_DEFAULT_REQUEST_RESPONSE_SIZE;
	if (response_size == 0) response_size = MODULE_DEFAULT_REQUEST_RESPONSE_SIZE;
	module->max_request_size  = request_size;
	module->max_response_size = response_size;
	if (request_size > response_size) {
		module->max_request_or_response_size = round_up_to_page(request_size);
	} else {
		module->max_request_or_response_size = round_up_to_page(response_size);
	}

	/* Table initialization calls a function that runs within the sandbox. Rather than setting the current sandbox,
	 * we partially fake this out by only setting the module_indirect_table and then clearing after table
	 * initialization is complete.
	 *
	 * assumption: This approach depends on module_new only being invoked at program start before preemption is
	 * enabled. We are check that local_sandbox_context_cache.module_indirect_table is NULL to gain confidence that
	 * we are not invoking this in a way that clobbers a current module.
	 *
	 * If we want to be able to do this later, we can possibly defer module_initialize_table until the first
	 * invocation. Alternatively, we can maintain the module_indirect_table per sandbox and call initialize
	 * on each sandbox if this "assumption" is too restrictive and we're ready to pay a per-sandbox performance hit.
	 */

	assert(local_sandbox_context_cache.module_indirect_table == NULL);
	local_sandbox_context_cache.module_indirect_table = module->indirect_table;
	module_initialize_table(module);
	local_sandbox_context_cache.module_indirect_table = NULL;

	/* Start listening for requests */
	rc = module_listen(module);
	if (rc < 0) goto err_listen;

done:
	return module;

err_listen:
dl_error:
	dlclose(module->dynamic_library_handle);
dl_open_error:
	free(module);
err:
	module = NULL;
	goto done;
}

/**
 * Parses a JSON file and allocates one or more new modules
 * @param file_name The path of the JSON file
 * @return RC 0 on Success. -1 on Error
 */
int
module_new_from_json(char *file_name)
{
	assert(file_name != NULL);
	int return_code = -1;

	/* Use stat to get file attributes and make sure file is there and OK */
	struct stat stat_buffer;
	memset(&stat_buffer, 0, sizeof(struct stat));
	errno = 0;
	if (stat(file_name, &stat_buffer) < 0) {
		fprintf(stderr, "Attempt to stat %s failed: %s\n", file_name, strerror(errno));
		goto err;
	}

	/* Open the file */
	errno             = 0;
	FILE *module_file = fopen(file_name, "r");
	if (!module_file) {
		fprintf(stderr, "Attempt to open %s failed: %s\n", file_name, strerror(errno));
		goto err;
	}

	/* Initialize a Buffer */
	assert(stat_buffer.st_size != 0);
	errno             = 0;
	char *file_buffer = malloc(stat_buffer.st_size);
	if (file_buffer == NULL) {
		fprintf(stderr, "Attempt to allocate file buffer failed: %s\n", strerror(errno));
		goto stat_buffer_alloc_err;
	}
	memset(file_buffer, 0, stat_buffer.st_size);

	/* Read the file into the buffer and check that the buffer size equals the file size */
	errno                = 0;
	int total_chars_read = fread(file_buffer, sizeof(char), stat_buffer.st_size, module_file);
#ifdef LOG_MODULE_LOADING
	debuglog("size read: %d content: %s\n", total_chars_read, file_buffer);
#endif
	if (total_chars_read != stat_buffer.st_size) {
		fprintf(stderr, "Attempt to read %s into buffer failed: %s\n", file_name, strerror(errno));
		goto fread_err;
	}
	assert(total_chars_read > 0);

	/* Close the file */
	errno = 0;
	if (fclose(module_file) == EOF) {
		fprintf(stderr, "Attempt to close buffer containing %s failed: %s\n", file_name, strerror(errno));
		goto fclose_err;
	};
	module_file = NULL;

	/* Initialize the Jasmine Parser and an array to hold the tokens */
	jsmn_parser module_parser;
	jsmn_init(&module_parser);
	jsmntok_t tokens[JSON_MAX_ELEMENT_SIZE * JSON_MAX_ELEMENT_COUNT];

	/* Use Jasmine to parse the JSON */
	int total_tokens = jsmn_parse(&module_parser, file_buffer, total_chars_read, tokens,
	                              sizeof(tokens) / sizeof(tokens[0]));
	if (total_tokens < 0) {
		if (total_tokens == JSMN_ERROR_INVAL) {
			fprintf(stderr, "Error parsing %s: bad token, JSON string is corrupted\n", file_name);
		} else if (total_tokens == JSMN_ERROR_PART) {
			fprintf(stderr, "Error parsing %s: JSON string is too short, expecting more JSON data\n",
			        file_name);
		} else if (total_tokens == JSMN_ERROR_NOMEM) {
			/*
			 * According to the README at https://github.com/zserge/jsmn, this is a potentially recoverable
			 * error. More tokens can be allocated and jsmn_parse can be re-invoked.
			 */
			fprintf(stderr, "Error parsing %s: Not enough tokens, JSON string is too large\n", file_name);
		}
		goto json_parse_err;
	}

	int module_count = 0;
	for (int i = 0; i < total_tokens; i++) {
		assert(tokens[i].type == JSMN_OBJECT);

		char module_name[MODULE_MAX_NAME_LENGTH] = { 0 };
		char module_path[MODULE_MAX_PATH_LENGTH] = { 0 };

		errno = 0;

		int32_t  request_size                                        = 0;
		int32_t  response_size                                       = 0;
		uint32_t port                                                = 0;
		uint32_t relative_deadline_us                                = 0;
		uint32_t expected_execution_us                               = 0;
		int      admissions_percentile                               = 50;
		int      j                                                   = 1;
		int      ntoks                                               = 2 * tokens[i].size;
		char     response_content_type[HTTP_MAX_HEADER_VALUE_LENGTH] = { 0 };

		for (; j < ntoks;) {
			int  ntks     = 1;
			char key[32]  = { 0 };
			char val[256] = { 0 };

			sprintf(val, "%.*s", tokens[j + i + 1].end - tokens[j + i + 1].start,
			        file_buffer + tokens[j + i + 1].start);
			sprintf(key, "%.*s", tokens[j + i].end - tokens[j + i].start,
			        file_buffer + tokens[j + i].start);

			if (strlen(key) == 0) panic("Unexpected encountered empty key\n");
			if (strlen(val) == 0) panic("%s field contained empty string\n", key);

			if (strcmp(key, "name") == 0) {
				// TODO: Currently, multiple modules can have identical names. Ports are the true unique
				// identifiers. Consider enforcing unique names in future
				strcpy(module_name, val);
			} else if (strcmp(key, "path") == 0) {
				// Invalid path will crash on dlopen
				strcpy(module_path, val);
			} else if (strcmp(key, "port") == 0) {
				// Validate sane port
				// If already taken, will error on bind call in module_listen
				int buffer = atoi(val);
				if (buffer < 0 || buffer > 65535)
					panic("Expected port between 0 and 65535, saw %d\n", buffer);
				port = buffer;
			} else if (strcmp(key, "relative-deadline-us") == 0) {
				int64_t buffer = strtoll(val, NULL, 10);
				if (buffer < 0 || buffer > (int64_t)RUNTIME_RELATIVE_DEADLINE_US_MAX)
					panic("Relative-deadline-us must be between 0 and %ld, was %ld\n",
					      (int64_t)RUNTIME_RELATIVE_DEADLINE_US_MAX, buffer);
				relative_deadline_us = (uint32_t)buffer;
			} else if (strcmp(key, "expected-execution-us") == 0) {
				int64_t buffer = strtoll(val, NULL, 10);
				if (buffer < 0 || buffer > (int64_t)RUNTIME_EXPECTED_EXECUTION_US_MAX)
					panic("Relative-deadline-us must be between 0 and %ld, was %ld\n",
					      (int64_t)RUNTIME_EXPECTED_EXECUTION_US_MAX, buffer);
				expected_execution_us = (uint32_t)buffer;
			} else if (strcmp(key, "admissions-percentile") == 0) {
				int32_t buffer = strtol(val, NULL, 10);
				if (buffer > 99 || buffer < 50)
					panic("admissions-percentile must be > 50 and <= 99 but was %d\n", buffer);
				admissions_percentile = (int)buffer;
			} else if (strcmp(key, "http-req-size") == 0) {
				int64_t buffer = strtoll(val, NULL, 10);
				if (buffer < 0 || buffer > RUNTIME_HTTP_REQUEST_SIZE_MAX)
					panic("http-req-size must be between 0 and %ld, was %ld\n",
					      (int64_t)RUNTIME_HTTP_REQUEST_SIZE_MAX, buffer);
				request_size = (int32_t)buffer;
			} else if (strcmp(key, "http-resp-size") == 0) {
				int64_t buffer = strtoll(val, NULL, 10);
				if (buffer < 0 || buffer > RUNTIME_HTTP_REQUEST_SIZE_MAX)
					panic("http-resp-size must be between 0 and %ld, was %ld\n",
					      (int64_t)RUNTIME_HTTP_REQUEST_SIZE_MAX, buffer);
				response_size = (int32_t)buffer;
			} else if (strcmp(key, "http-resp-content-type") == 0) {
				if (strlen(val) == 0) panic("http-resp-content-type was unexpectedly an empty string");
				strcpy(response_content_type, val);
			} else {
#ifdef LOG_MODULE_LOADING
				debuglog("Invalid (%s,%s)\n", key, val);
#endif
			}
			j += ntks;
		}
		i += ntoks;


		/* Validate presence of required fields */
		if (strlen(module_name) == 0) panic("name field is required\n");
		if (strlen(module_path) == 0) panic("path field is required\n");
		if (port == 0) panic("port field is required\n");

#ifdef ADMISSIONS_CONTROL
		/* expected-execution-us and relative-deadline-us are required in case of admissions control */
		if (expected_execution_us == 0) panic("expected-execution-us is required\n");
		if (relative_deadline_us == 0) panic("relative_deadline_us is required\n");

		/* If the ratio is too big, admissions control is too coarse */
		uint32_t ratio = relative_deadline_us / expected_execution_us;
		if (ratio > ADMISSIONS_CONTROL_GRANULARITY)
			panic("Ratio of Deadline to Execution time cannot exceed admissions control "
			      "granularity of "
			      "%d\n",
			      ADMISSIONS_CONTROL_GRANULARITY);
#else
		/* relative-deadline-us is required if scheduler is EDF */
		if (scheduler == SCHEDULER_EDF && relative_deadline_us == 0)
			panic("relative_deadline_us is required\n");
#endif

		/* Allocate a module based on the values from the JSON */
		struct module *module = module_new(module_name, module_path, 0, 0, relative_deadline_us, port,
		                                   request_size, response_size, admissions_percentile,
		                                   expected_execution_us);
		if (module == NULL) goto module_new_err;

		assert(module);
		module_set_http_info(module, response_content_type);
		module_count++;
	}

	if (module_count == 0) panic("%s contained no active modules\n", file_name);
#ifdef LOG_MODULE_LOADING
	debuglog("Loaded %d module%s!\n", module_count, module_count > 1 ? "s" : "");
#endif
	free(file_buffer);

	return_code = 0;

done:
	return return_code;
module_new_err:
json_parse_err:
fclose_err:
	/* We will retry fclose when we fall through into stat_buffer_alloc_err */
fread_err:
	free(file_buffer);
stat_buffer_alloc_err:
	// Check to ensure we haven't already close this
	if (module_file != NULL) {
		if (fclose(module_file) == EOF) panic("Failed to close file\n");
	}
err:
	return_code = -1;
	goto done;
}
