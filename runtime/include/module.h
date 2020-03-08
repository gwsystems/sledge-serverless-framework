#ifndef SFRT_MODULE_H
#define SFRT_MODULE_H

#include <uv.h>
#include "types.h"

struct module {
	char name[MOD_NAME_MAX];
	char path[MOD_PATH_MAX];
	void *dynamic_library_handle; // Handle to the *.so of the serverless function
	i32 argument_count;
	u32 stack_size; // a specification?
	u64 max_memory; // perhaps a specification of the module. (max 4GB)
	u32 timeout;    // again part of the module specification.
	u32 reference_count; // ref count how many instances exist here.
	struct indirect_table_entry indirect_table[INDIRECT_TABLE_SIZE];
	struct sockaddr_in socket_address;
	int                socket_descriptor;
	int                port;

	// unfortunately, using UV for accepting connections is not great!
	// on_connection, to create a new accepted connection, will have to
	// init a tcp handle, which requires a uvloop. cannot use main as
	// rest of the connection is handled in sandboxing threads, with per-core(per-thread) tls data-structures.
	// so, using direct epoll for accepting connections.
	//	uv_handle_t srvuv;

	unsigned long max_request_size;
	char          request_headers[HTTP_HEADERS_MAX][HTTP_HEADER_MAXSZ];
	int           request_header_count;
	char          request_content_type[HTTP_HEADERVAL_MAXSZ];

	// resp size including headers!
	unsigned long max_response_size;
	int           response_header_count;
	char          response_content_type[HTTP_HEADERVAL_MAXSZ];
	char          response_headers[HTTP_HEADERS_MAX][HTTP_HEADER_MAXSZ];
	
	// Equals the largest of either max_request_size or max_response_size
	unsigned long max_request_or_response_size;

	// Functions to initialize aspects of sandbox
	mod_glb_fn_t  initialize_globals;
	mod_mem_fn_t  initialize_memory;
	mod_tbl_fn_t  initialize_tables;
	mod_libc_fn_t initialize_libc;

	// Entry Function to invoke serverless function
	mod_main_fn_t main;
};

struct module *module_find_by_name(char *name);
struct module *module_find_by_socket_descriptor(int socket_descriptor);
struct module *module_alloc(char *mod_name, char *mod_path, i32 argument_count, u32 stack_sz, u32 max_heap, u32 timeout, int port, int req_sz, int resp_sz);
void           module_free(struct module *module);


/**
 * Sets the HTTP Request and Response Headers and Content type on a module
 * @param module
 * @param request_count
 * @param request_headers
 * @param request_content_type
 * @param response_count
 * @param response_headers
 * @param response_content_type
 **/
static inline void
module_http_info(struct module *module, int request_count, char *request_headers, char request_content_type[],
                 int response_count, char *response_headers, char response_content_type[])
{
	assert(module);
	module->request_header_count = request_count;
	memcpy(module->request_headers, request_headers, HTTP_HEADER_MAXSZ * HTTP_HEADERS_MAX);
	strcpy(module->request_content_type, request_content_type);
	module->response_header_count = response_count;
	memcpy(module->response_headers, response_headers, HTTP_HEADER_MAXSZ * HTTP_HEADERS_MAX);
	strcpy(module->response_content_type, response_content_type);
}

/**
 * Validate module, defined as having a non-NULL dynamical library handle and entry function pointer
 * @param module
 * @return 1 if valid. 0 if invalid
 **/
static inline int
module_is_valid(struct module *module)
{
	if (module && module->dynamic_library_handle && module->main) return 1;
	return 0;
}

// TODO: What is the point of these wrapper functions. They seem exceptionally silly
// We have the module, so why not just invoke the function pointer directly?

/**
 * Invoke a module's initialize_globals
 * @param module
 **/
static inline void
module_globals_init(struct module *module)
{
	// called in a sandbox.
	module->initialize_globals();
}

/**
 * Invoke a module's initialize_tables
 * @param module
 **/
static inline void
module_table_init(struct module *module)
{
	// called at module creation time (once only per module).
	module->initialize_tables();
}

/**
 * Invoke a module's initialize_libc
 * @param module
 **/
static inline void
module_libc_init(struct module *module, i32 env, i32 arguments)
{
	// called in a sandbox.
	module->initialize_libc(env, arguments);
}

/**
 * Invoke a module's initialize_memory
 * @param module
 **/
static inline void
module_memory_init(struct module *module)
{
	// called in a sandbox.
	module->initialize_memory();
}

/**
 * Invoke a module's entry function, forwarding on argc and argv
 * @param module
 * @param argc standard UNIX count of arguments
 * @param argv standard UNIX vector of arguments
 **/
static inline i32
module_entry(struct module *module, i32 argc, i32 argv)
{
	return module->main(argc, argv);
}

/**
 * Increment a modules reference count
 * @param module
 **/
static inline void
module_acquire(struct module *module)
{
	// TODO: atomic.
	module->reference_count++;
}

/**
 * Decrement a modules reference count
 * @param module
 **/
static inline void
module_release(struct module *module)
{
	// TODO: atomic.
	module->reference_count--;
}

/**
 * Get a module's argument count
 * @param module
 * @returns the number of arguments
 **/
static inline i32
module_argument_count(struct module *module)
{
	return module->argument_count;
}

#endif /* SFRT_MODULE_H */
