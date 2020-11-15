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
#include "module.h"
#include "module_database.h"
#include "panic.h"
#include "runtime.h"

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
	if (socket_descriptor < 0) goto err_create_socket;


	/* Configure the socket to allow multiple sockets to bind to the same host and port */
	int optval = 1;
	rc         = setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	if (rc < 0) goto err_set_socket_option;
	optval = 1;
	rc     = setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (rc < 0) goto err_set_socket_option;

	/* Bind name [all addresses]:[module->port] to socket */
	module->socket_descriptor              = socket_descriptor;
	module->socket_address.sin_family      = AF_INET;
	module->socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
	module->socket_address.sin_port        = htons((unsigned short)module->port);
	rc = bind(socket_descriptor, (struct sockaddr *)&module->socket_address, sizeof(module->socket_address));
	if (rc < 0) goto err_bind_socket;

	/* Listen to the interface */
	rc = listen(socket_descriptor, MODULE_MAX_PENDING_CLIENT_REQUESTS);
	if (rc < 0) goto err_listen;


	/* Set the socket descriptor and register with our global epoll instance to monitor for incoming HTTP
	requests */
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)module;
	accept_evt.events   = EPOLLIN;
	rc = epoll_ctl(runtime_epoll_file_descriptor, EPOLL_CTL_ADD, module->socket_descriptor, &accept_evt);
	if (rc < 0) goto err_add_to_epoll;

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
 * @param argument_count
 * @param stack_size
 * @param max_memory
 * @param relative_deadline_us
 * @param port
 * @param request_size
 * @returns A new module or NULL in case of failure
 */

struct module *
module_new(char *name, char *path, int32_t argument_count, uint32_t stack_size, uint32_t max_memory,
           uint32_t relative_deadline_us, int port, int request_size, int response_size, int admissions_percentile,
           uint32_t expected_execution_us)
{
	int rc = 0;

	errno                 = 0;
	struct module *module = (struct module *)malloc(sizeof(struct module));
	if (!module) {
		fprintf(stderr, "Failed to allocate module: %s\n", strerror(errno));
		goto err;
	};

	memset(module, 0, sizeof(struct module));

	/* Load the dynamic library *.so file with lazy function call binding and deep binding */
	module->dynamic_library_handle = dlopen(path, RTLD_LAZY | RTLD_DEEPBIND);
	if (module->dynamic_library_handle == NULL) {
		fprintf(stderr, "Failed to open dynamic library at %s: %s\n", path, dlerror());
		goto dl_open_error;
	};

	/* Resolve the symbols in the dynamic library *.so file */
	module->main = (mod_main_fn_t)dlsym(module->dynamic_library_handle, MODULE_MAIN);
	if (module->main == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s: %s\n", MODULE_MAIN, dlerror());
		goto dl_error;
	}

	module->initialize_globals = (mod_glb_fn_t)dlsym(module->dynamic_library_handle, MODULE_INITIALIZE_GLOBALS);
	if (module->initialize_globals == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s: %s\n", MODULE_INITIALIZE_GLOBALS, dlerror());
		goto dl_error;
	}

	module->initialize_memory = (mod_mem_fn_t)dlsym(module->dynamic_library_handle, MODULE_INITIALIZE_MEMORY);
	if (module->initialize_memory == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s: %s\n", MODULE_INITIALIZE_MEMORY, dlerror());
		goto dl_error;
	};

	module->initialize_tables = (mod_tbl_fn_t)dlsym(module->dynamic_library_handle, MODULE_INITIALIZE_TABLE);
	if (module->initialize_tables == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s: %s\n", MODULE_INITIALIZE_TABLE, dlerror());
		goto dl_error;
	};

	module->initialize_libc = (mod_libc_fn_t)dlsym(module->dynamic_library_handle, MODULE_INITIALIZE_LIBC);
	if (module->initialize_libc == NULL) {
		fprintf(stderr, "Failed to resolve symbol %s: %s\n", MODULE_INITIALIZE_LIBC, dlerror());
		goto dl_error;
	}

	/* Set fields in the module struct */
	strncpy(module->name, name, MODULE_MAX_NAME_LENGTH);
	strncpy(module->path, path, MODULE_MAX_PATH_LENGTH);

	module->argument_count    = argument_count;
	module->stack_size        = round_up_to_page(stack_size == 0 ? WASM_STACK_SIZE : stack_size);
	module->max_memory        = max_memory == 0 ? ((uint64_t)WASM_PAGE_SIZE * WASM_MAX_PAGES) : max_memory;
	module->socket_descriptor = -1;
	module->port              = port;

	/* Deadlines */
	module->relative_deadline_us = relative_deadline_us;

	/* This should have been handled when a module was loaded */
	assert(relative_deadline_us < runtime_relative_deadline_us_max);

	/* This can overflow a uint32_t, so be sure to cast appropriately */
	module->relative_deadline = (uint64_t)relative_deadline_us * runtime_processor_speed_MHz;

	/* Admissions Control */
	uint64_t expected_execution = expected_execution_us * runtime_processor_speed_MHz;
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

	int   module_count    = 0;
	char *request_headers = NULL;
	char *reponse_headers = NULL;
	for (int i = 0; i < total_tokens; i++) {
		assert(tokens[i].type == JSMN_OBJECT);

		char module_name[MODULE_MAX_NAME_LENGTH] = { 0 };
		char module_path[MODULE_MAX_PATH_LENGTH] = { 0 };

		errno           = 0;
		request_headers = (char *)malloc(HTTP_MAX_HEADER_LENGTH * HTTP_MAX_HEADER_COUNT);
		if (request_headers == NULL) {
			fprintf(stderr, "Attempt to allocate request headers failed: %s\n", strerror(errno));
			goto request_headers_alloc_err;
		}
		memset(request_headers, 0, HTTP_MAX_HEADER_LENGTH * HTTP_MAX_HEADER_COUNT);

		errno           = 0;
		reponse_headers = (char *)malloc(HTTP_MAX_HEADER_LENGTH * HTTP_MAX_HEADER_COUNT);
		if (reponse_headers == NULL) {
			fprintf(stderr, "Attempt to allocate response headers failed: %s\n", strerror(errno));
			goto response_headers_alloc_err;
		}
		memset(reponse_headers, 0, HTTP_MAX_HEADER_LENGTH * HTTP_MAX_HEADER_COUNT);

		int32_t  request_size                                        = 0;
		int32_t  response_size                                       = 0;
		int32_t  argument_count                                      = 0;
		uint32_t port                                                = 0;
		uint32_t relative_deadline_us                                = 0;
		uint32_t expected_execution_us                               = 0;
		int      admissions_percentile                               = 50;
		bool     is_active                                           = false;
		int32_t  request_count                                       = 0;
		int32_t  response_count                                      = 0;
		int      j                                                   = 1;
		int      ntoks                                               = 2 * tokens[i].size;
		char     request_content_type[HTTP_MAX_HEADER_VALUE_LENGTH]  = { 0 };
		char     response_content_type[HTTP_MAX_HEADER_VALUE_LENGTH] = { 0 };

		for (; j < ntoks;) {
			int  ntks     = 1;
			char key[32]  = { 0 };
			char val[256] = { 0 };

			sprintf(val, "%.*s", tokens[j + i + 1].end - tokens[j + i + 1].start,
			        file_buffer + tokens[j + i + 1].start);
			sprintf(key, "%.*s", tokens[j + i].end - tokens[j + i].start,
			        file_buffer + tokens[j + i].start);
			if (strcmp(key, "name") == 0) {
				strcpy(module_name, val);
			} else if (strcmp(key, "path") == 0) {
				strcpy(module_path, val);
			} else if (strcmp(key, "port") == 0) {
				port = atoi(val);
			} else if (strcmp(key, "argsize") == 0) {
				argument_count = atoi(val);
			} else if (strcmp(key, "active") == 0) {
				is_active = (strcmp(val, "yes") == 0);
			} else if (strcmp(key, "relative-deadline-us") == 0) {
				unsigned long long buffer = strtoull(val, NULL, 10);
				if (buffer > runtime_relative_deadline_us_max)
					panic("Max relative-deadline-us is %u, but entry was %llu\n", UINT32_MAX,
					      buffer);
				relative_deadline_us = (uint32_t)buffer;
			} else if (strcmp(key, "expected-execution-us") == 0) {
				unsigned long long buffer = strtoull(val, NULL, 10);
				if (buffer > UINT32_MAX)
					panic("Max expected-execution-us is %u, but entry was %llu\n", UINT32_MAX,
					      buffer);

				expected_execution_us = (uint32_t)buffer;
			} else if (strcmp(key, "admissions-percentile") == 0) {
				unsigned long long buffer = strtoull(val, NULL, 10);
				if (buffer > 99 || buffer < 50)
					panic("admissions-percentile must be > 50 and <= 99 but was %llu\n", buffer);

				admissions_percentile = (int)buffer;
			} else if (strcmp(key, "http-req-headers") == 0) {
				assert(tokens[i + j + 1].type == JSMN_ARRAY);
				assert(tokens[i + j + 1].size <= HTTP_MAX_HEADER_COUNT);

				request_count = tokens[i + j + 1].size;
				ntks += request_count;
				ntoks += request_count;
				for (int k = 1; k <= tokens[i + j + 1].size; k++) {
					jsmntok_t *g = &tokens[i + j + k + 1];
					char *     r = request_headers + ((k - 1) * HTTP_MAX_HEADER_LENGTH);
					assert(g->end - g->start < HTTP_MAX_HEADER_LENGTH);
					strncpy(r, file_buffer + g->start, g->end - g->start);
				}
			} else if (strcmp(key, "http-resp-headers") == 0) {
				assert(tokens[i + j + 1].type == JSMN_ARRAY);
				assert(tokens[i + j + 1].size <= HTTP_MAX_HEADER_COUNT);

				response_count = tokens[i + j + 1].size;
				ntks += response_count;
				ntoks += response_count;
				for (int k = 1; k <= tokens[i + j + 1].size; k++) {
					jsmntok_t *g = &tokens[i + j + k + 1];
					char *     r = reponse_headers + ((k - 1) * HTTP_MAX_HEADER_LENGTH);
					assert(g->end - g->start < HTTP_MAX_HEADER_LENGTH);
					strncpy(r, file_buffer + g->start, g->end - g->start);
				}
			} else if (strcmp(key, "http-req-size") == 0) {
				request_size = atoi(val);
			} else if (strcmp(key, "http-resp-size") == 0) {
				response_size = atoi(val);
			} else if (strcmp(key, "http-req-content-type") == 0) {
				strcpy(request_content_type, val);
			} else if (strcmp(key, "http-resp-content-type") == 0) {
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
#ifdef ADMISSIONS_CONTROL
		if (expected_execution_us == 0) panic("expected-execution-us is required for EDF\n");
#endif

		if (is_active) {
			/* Allocate a module based on the values from the JSON */
			struct module *module = module_new(module_name, module_path, argument_count, 0, 0,
			                                   relative_deadline_us, port, request_size, response_size,
			                                   admissions_percentile, expected_execution_us);
			if (module == NULL) goto module_new_err;

			assert(module);
			module_set_http_info(module, request_count, request_headers, request_content_type,
			                     response_count, reponse_headers, response_content_type);
			module_count++;
		}

		free(request_headers);
		free(reponse_headers);
	}

	if (module_count == 0) fprintf(stderr, "%s contained no active modules\n", file_name);
#ifdef LOG_MODULE_LOADING
	debuglog("Loaded %d module%s!\n", module_count, module_count > 1 ? "s" : "");
#endif
	free(file_buffer);

	return_code = 0;

done:
	return return_code;
module_new_err:
response_headers_alloc_err:
	free(request_headers);
request_headers_alloc_err:
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
