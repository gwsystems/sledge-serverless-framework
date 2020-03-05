#include <runtime.h>
#include <module.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <uv.h>
#include <util.h>

// In-memory representation of all active modules
static struct module *__mod_db[MOD_MAX] = { NULL };
static int            __mod_free_off    = 0;

/**
 * Given a name, find the associated module
 * @param name
 * @return module or NULL if no match found
 **/
struct module *
module_find_by_name(char *name)
{
	int f = __mod_free_off;
	for (int i = 0; i < f; i++) {
		assert(__mod_db[i]);
		if (strcmp(__mod_db[i]->name, name) == 0) return __mod_db[i];
	}
	return NULL;
}

/**
 * Given a socket_descriptor, find the associated module
 * @param socket_descriptor
 * @return module or NULL if no match found
 **/
struct module *
module_find_by_socket_descriptor(int socket_descriptor)
{
	int f = __mod_free_off;
	for (int i = 0; i < f; i++) {
		assert(__mod_db[i]);
		if (__mod_db[i]->socket_descriptor == socket_descriptor) return __mod_db[i];
	}
	return NULL;
}

/**
 * Adds a module to the in-memory module DB
 * @param module module to add
 * @return 0 on success. Aborts program on failure
 **/
static inline int
module_add(struct module *module)
{
	assert(module->socket_descriptor == -1);

	int f = __sync_fetch_and_add(&__mod_free_off, 1);
	assert(f < MOD_MAX);
	__mod_db[f] = module;

	return 0;
}

/**
 * Start the module as a server listening at module->port
 * @param module
 **/
static inline void
module_server_init(struct module *module)
{
	// Allocate a new socket
	int socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
	assert(socket_descriptor > 0);

	// Configure socket address as [all addresses]:[module->port]
	module->socket_address.sin_family      = AF_INET;
	module->socket_address.sin_addr.s_addr = htonl(INADDR_ANY);
	module->socket_address.sin_port        = htons((unsigned short)module->port);

	// Configure the socket to allow multiple sockets to bind to the same host and port
	int optval = 1;
	setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	optval = 1;
	setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	// Bind to the interface
	if (bind(socket_descriptor, (struct sockaddr *)&module->socket_address, sizeof(module->socket_address)) < 0) {
		perror("bind");
		assert(0);
	}

	// Listen to the interface? Check that it is live?
	if (listen(socket_descriptor, MOD_BACKLOG) < 0) assert(0);

	module->socket_descriptor = socket_descriptor;

	// Register the socket descriptor with our global epoll instance to monitor for incoming HTTP requests
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)module;
	accept_evt.events   = EPOLLIN;
	if (epoll_ctl(epoll_file_descriptor, EPOLL_CTL_ADD, module->socket_descriptor, &accept_evt) < 0) assert(0);
}

/**
 * Module Mega Setup Function
 * Creates a new module, invokes tbl_init_fn to initialize the indirect table, adds it to the module DB, and starts listening for HTTP Requests
 * 
 * @param name
 * @param path
 * @param argument_count
 * @param stack_size
 * @param max_memory
 * @param timeout
 * @param port
 * @param request_size
 * @returns A new module or NULL in case of failure
 **/
struct module *
module_alloc(char *name, char *path, i32 argument_count, u32 stack_size, u32 max_memory, u32 timeout, int port, int request_size,
             int response_size)
{
	struct module *module = (struct module *)malloc(sizeof(struct module));
	if (!module) return NULL;

	memset(module, 0, sizeof(struct module));

	// Load the dynamic library *.so file with lazy function call binding and deep binding
	module->dynamic_library_handle = dlopen(path, RTLD_LAZY | RTLD_DEEPBIND);
	if (module->dynamic_library_handle == NULL) goto dl_open_error;

	// Resolve the symbols in the dynamic library *.so file
	module->entry_fn = (mod_main_fn_t)dlsym(module->dynamic_library_handle, MOD_MAIN_FN);
	if (module->entry_fn == NULL) goto dl_error;

	module->glb_init_fn = (mod_glb_fn_t)dlsym(module->dynamic_library_handle, MOD_GLB_FN);
	if (module->glb_init_fn == NULL) goto dl_error;

	module->mem_init_fn = (mod_mem_fn_t)dlsym(module->dynamic_library_handle, MOD_MEM_FN);
	if (module->mem_init_fn == NULL) goto dl_error;

	module->tbl_init_fn = (mod_tbl_fn_t)dlsym(module->dynamic_library_handle, MOD_TBL_FN);
	if (module->tbl_init_fn == NULL) goto dl_error;

	module->libc_init_fn = (mod_libc_fn_t)dlsym(module->dynamic_library_handle, MOD_LIBC_FN);
	if (module->libc_init_fn == NULL) goto dl_error;

	// Set fields in the module struct
	strncpy(module->name, name, MOD_NAME_MAX);
	strncpy(module->path, path, MOD_PATH_MAX);

	module->argument_count      = argument_count;
	module->stack_size = round_up_to_page(stack_size == 0 ? WASM_STACK_SIZE : stack_size);
	module->max_memory = max_memory == 0 ? ((u64)WASM_PAGE_SIZE * WASM_MAX_PAGES) : max_memory;
	module->timeout    = timeout;
	module->socket_descriptor = -1;
	module->port = port;
	if (request_size == 0) request_size = MOD_REQ_RESP_DEFAULT;
	if (response_size == 0) response_size = MOD_REQ_RESP_DEFAULT;
	module->max_request_size  = request_size;
	module->max_response_size = response_size;
	module->max_request_or_response_size   = round_up_to_page(request_size > response_size ? request_size : response_size);

	// module_indirect_table is a thread-local struct
	struct indirect_table_entry *cache_tbl = module_indirect_table;
	
	// assumption: All modules are created at program start before we enable preemption or enable the execution of any worker threads
	// We are checking that thread-local module_indirect_table is NULL to prove that we aren't yet preempting
	// If we want to be able to do this later, we can possibly defer module_table_init until the first invocation 
	assert(cache_tbl == NULL);
	
	// TODO: determine why we have to set the module_indirect_table state before calling table init and then restore the existing value
	// What is the relationship between these things?
	module_indirect_table = module->indirect_table;
	module_table_init(module);
	module_indirect_table = cache_tbl;

	// Add the module to the in-memory module DB
	module_add(module);

	// Start listening for requests
	module_server_init(module);

	return module;

dl_error:
	dlclose(module->dynamic_library_handle);

dl_open_error:
	free(module);
	debuglog("%s\n", dlerror());
	return NULL;
}

/**
 * Module Mega Teardown Function
 * Closes the socket and dynamic library, and then frees the module
 * @param module - the module to teardown
 **/
void
module_free(struct module *module)
{
	if (module == NULL) return;
	if (module->dynamic_library_handle == NULL) return;

	// Do not free if we still have oustanding references
	if (module->reference_count) return;

	// TODO: What about the module database? Do we need to do any cleanup there?

	close(module->socket_descriptor);
	dlclose(module->dynamic_library_handle);
	free(module);
}
