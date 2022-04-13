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
#include "wasm_table.h"

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
 * Sets the HTTP Response Content type on a module
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

	/* Do not free if we still have oustanding references */
	if (module->reference_count) return;

	close(module->socket_descriptor);
	sledge_abi_symbols_deinit(&module->abi);
	free(module);
}

static inline int
module_init(struct module *module, char *name, char *path, uint32_t stack_size, uint32_t relative_deadline_us, int port,
            int request_size, int response_size, int admissions_percentile, uint32_t expected_execution_us)
{
	assert(module != NULL);

	int rc = 0;

	atomic_init(&module->reference_count, 0);

	rc = sledge_abi_symbols_init(&module->abi, path);
	if (rc != 0) goto err;

	/* Set fields in the module struct */
	strncpy(module->name, name, MODULE_MAX_NAME_LENGTH);
	strncpy(module->path, path, MODULE_MAX_PATH_LENGTH);

	module->stack_size        = ((uint32_t)(round_up_to_page(stack_size == 0 ? WASM_STACK_SIZE : stack_size)));
	module->socket_descriptor = -1;
	module->port              = port;

	/* Deadlines */
	module->relative_deadline_us = relative_deadline_us;

	/* This can overflow a uint32_t, so be sure to cast appropriately */
	module->relative_deadline = (uint64_t)relative_deadline_us * runtime_processor_speed_MHz;

	/* Admissions Control */
	uint64_t expected_execution = (uint64_t)expected_execution_us * runtime_processor_speed_MHz;
	admissions_info_initialize(&module->admissions_info, admissions_percentile, expected_execution,
	                           module->relative_deadline);

	/* Request Response Buffer */
	if (request_size == 0) request_size = MODULE_DEFAULT_REQUEST_RESPONSE_SIZE;
	if (response_size == 0) response_size = MODULE_DEFAULT_REQUEST_RESPONSE_SIZE;
	module->max_request_size  = round_up_to_page(request_size);
	module->max_response_size = round_up_to_page(response_size);

	module_alloc_table(module);
	module_initialize_pools(module);

	/* Start listening for requests */
	rc = module_listen(module);
	if (rc < 0) goto err;

done:
	return rc;
err:
	rc = -1;
	goto done;
}

/**
 * Module Contructor
 * Creates a new module, invokes initialize_tables to initialize the indirect table, adds it to the module DB, and
 *starts listening for HTTP Requests
 *
 * @param name
 * @param path
 * @param stack_size
 * @param relative_deadline_us
 * @param port
 * @param request_size
 * @returns A new module or NULL in case of failure
 */

struct module *
module_alloc(char *name, char *path, uint32_t stack_size, uint32_t relative_deadline_us, int port, int request_size,
             int response_size, int admissions_percentile, uint32_t expected_execution_us)
{
	/* Validate presence of required fields */
	if (strlen(name) == 0) panic("name field is required\n");
	if (strlen(path) == 0) panic("path field is required\n");
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
	if (scheduler == SCHEDULER_EDF && relative_deadline_us == 0) panic("relative_deadline_us is required\n");
#endif

	struct module *module = (struct module *)calloc(1, sizeof(struct module));
	if (!module) {
		fprintf(stderr, "Failed to allocate module: %s\n", strerror(errno));
		goto err;
	};

	int rc = module_init(module, name, path, stack_size, relative_deadline_us, port, request_size, response_size,
	                     admissions_percentile, expected_execution_us);
	if (rc < 0) goto init_err;

done:
	return module;

init_err:
	free(module);
err:
	module = NULL;
	goto done;
}

/**
 * Allocates a buffer in memory containing the entire contents of the file provided
 * @param file_name file to load into memory
 * @param ret_ptr Pointer to set with address of buffer this function allocates. The caller must free this!
 * @return size of the allocated buffer or -1 in case of error;
 */
static inline size_t
load_file_into_buffer(char *file_name, char **file_buffer)
{
	/* Use stat to get file attributes and make sure file is present and not empty */
	struct stat stat_buffer;
	if (stat(file_name, &stat_buffer) < 0) {
		fprintf(stderr, "Attempt to stat %s failed: %s\n", file_name, strerror(errno));
		goto err;
	}
	if (stat_buffer.st_size == 0) {
		fprintf(stderr, "File %s is unexpectedly empty\n", file_name);
		goto err;
	}
	if (!S_ISREG(stat_buffer.st_mode)) {
		fprintf(stderr, "File %s is not a regular file\n", file_name);
		goto err;
	}

	/* Open the file */
	FILE *module_file = fopen(file_name, "r");
	if (!module_file) {
		fprintf(stderr, "Attempt to open %s failed: %s\n", file_name, strerror(errno));
		goto err;
	}

	/* Initialize a Buffer */
	*file_buffer = calloc(1, stat_buffer.st_size);
	if (*file_buffer == NULL) {
		fprintf(stderr, "Attempt to allocate file buffer failed: %s\n", strerror(errno));
		goto stat_buffer_alloc_err;
	}

	/* Read the file into the buffer and check that the buffer size equals the file size */
	ssize_t total_chars_read = fread(*file_buffer, sizeof(char), stat_buffer.st_size, module_file);
#ifdef LOG_MODULE_LOADING
	debuglog("size read: %d content: %s\n", total_chars_read, *file_buffer);
#endif
	if (total_chars_read != stat_buffer.st_size) {
		fprintf(stderr, "Attempt to read %s into buffer failed: %s\n", file_name, strerror(errno));
		goto fread_err;
	}
	assert(total_chars_read > 0);

	/* Close the file */
	if (fclose(module_file) == EOF) {
		fprintf(stderr, "Attempt to close buffer containing %s failed: %s\n", file_name, strerror(errno));
		goto fclose_err;
	};
	module_file = NULL;

	return total_chars_read;

fclose_err:
	/* We will retry fclose when we fall through into stat_buffer_alloc_err */
fread_err:
	free(*file_buffer);
stat_buffer_alloc_err:
	// Check to ensure we haven't already close this
	if (module_file != NULL) {
		if (fclose(module_file) == EOF) panic("Failed to close file\n");
	}
err:
	return (ssize_t)-1;
}

/**
 * Parses a JSON file and allocates one or more new modules
 * @param file_name The path of the JSON file
 * @return RC 0 on Success. -1 on Error
 */
int
module_alloc_from_json(char *file_name)
{
	assert(file_name != NULL);

	int       return_code = -1;
	jsmntok_t tokens[JSON_MAX_ELEMENT_SIZE * JSON_MAX_ELEMENT_COUNT];

	/* Load file_name into memory */
	char   *file_buffer      = NULL;
	ssize_t total_chars_read = load_file_into_buffer(file_name, &file_buffer);
	if (total_chars_read <= 0) goto module_alloc_err;

	/* Initialize the Jasmine Parser and an array to hold the tokens */
	jsmn_parser module_parser;
	jsmn_init(&module_parser);

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
		/* If we have multiple objects, they should be wrapped in a JSON array */
		if (tokens[i].type == JSMN_ARRAY) continue;
		assert(tokens[i].type == JSMN_OBJECT);

		char module_name[MODULE_MAX_NAME_LENGTH] = { 0 };
		char module_path[MODULE_MAX_PATH_LENGTH] = { 0 };

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

		/* Allocate a module based on the values from the JSON */
		struct module *module = module_alloc(module_name, module_path, 0, relative_deadline_us, port,
		                                     request_size, response_size, admissions_percentile,
		                                     expected_execution_us);
		if (module == NULL) goto module_alloc_err;

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
module_alloc_err:
json_parse_err:
file_load_err:
	free(file_buffer);
err:
	return_code = -1;
	goto done;
}
