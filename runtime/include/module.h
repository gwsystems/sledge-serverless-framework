#ifndef SFRT_MODULE_H
#define SFRT_MODULE_H

#include <uv.h>
#include "types.h"

struct module {
	char name[MOD_NAME_MAX];
	char path[MOD_PATH_MAX];

	void *        dynamic_library_handle; // Handle to the *.so of the serverless function
	mod_main_fn_t entry_fn;
	mod_glb_fn_t  glb_init_fn;
	mod_mem_fn_t  mem_init_fn;
	mod_tbl_fn_t  tbl_init_fn;
	mod_libc_fn_t libc_init_fn;

	struct indirect_table_entry indirect_table[INDIRECT_TABLE_SIZE];

	i32 argument_count;
	u32 stack_size; // a specification?
	u64 max_memory; // perhaps a specification of the module. (max 4GB)
	u32 timeout;    // again part of the module specification.

	u32 reference_count; // ref count how many instances exist here.

	struct sockaddr_in socket_address;
	int                socket_descriptor; 
	int 			   port;

	// unfortunately, using UV for accepting connections is not great!
	// on_connection, to create a new accepted connection, will have to
	// init a tcp handle, which requires a uvloop. cannot use main as
	// rest of the connection is handled in sandboxing threads, with per-core(per-thread) tls data-structures.
	// so, using direct epoll for accepting connections.
	//	uv_handle_t srvuv;

	// req/resp from http, (resp size including headers!)..
	unsigned long max_request_size;
	unsigned long max_response_size;
	// Equals the largest of either max_request_size or max_response_size
	unsigned long max_request_or_response_size; 
	int           request_header_count; 
	int			  response_header_count;
	char          request_headers[HTTP_HEADERS_MAX][HTTP_HEADER_MAXSZ];
	char          request_content_type[HTTP_HEADERVAL_MAXSZ];
	char          response_content_type[HTTP_HEADERVAL_MAXSZ];
	char          response_headers[HTTP_HEADERS_MAX][HTTP_HEADER_MAXSZ];
};

struct module *module_alloc(char *mod_name, char *mod_path, i32 argument_count, u32 stack_sz, u32 max_heap, u32 timeout,
                            int port, int req_sz, int resp_sz);
// frees only if reference_count == 0
void           module_free(struct module *module);
struct module *module_find_by_name(char *name);
struct module *module_find_by_socket_descriptor(int sock);


static inline void
module_http_info(
	struct module *module, 
	int request_count, 
	char *request_headers, 
	char request_content_type[], 
	int response_count, 
	char *response_headers, 
	char response_content_type[])
{
	assert(module);
	module->request_header_count  = request_count;
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
	if (module && module->dynamic_library_handle && module->entry_fn) return 1;

	return 0;
}

/**
 * Invoke a module's glb_init_fn
 * @param module
 **/
static inline void
module_globals_init(struct module *module)
{
	// called in a sandbox.
	module->glb_init_fn();
}

/**
 * Invoke a module's tbl_init_fn
 * @param module
 **/
static inline void
module_table_init(struct module *module)
{
	// called at module creation time (once only per module).
	module->tbl_init_fn();
}

/**
 * Invoke a module's libc_init_fn
 * @param module
 **/
static inline void
module_libc_init(struct module *module, i32 env, i32 args)
{
	// called in a sandbox.
	module->libc_init_fn(env, args);
}

/**
 * Invoke a module's mem_init_fn
 * @param module
 **/
static inline void
module_memory_init(struct module *module)
{
	// called in a sandbox.
	module->mem_init_fn();
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
	return module->entry_fn(argc, argv);
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
