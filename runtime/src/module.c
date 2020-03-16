#include <runtime.h>
#include <module.h>
#include <module_database.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <uv.h>
#include <util.h>

/***************************************
 * Private Static Inline
 ***************************************/

/**
 * Start the module as a server listening at module->port
 * @param module
 **/
static inline void
module__initialize_as_server(struct module *module)
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


	// Set the socket descriptor and register with our global epoll instance to monitor for incoming HTTP requests
	module->socket_descriptor = socket_descriptor;
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)module;
	accept_evt.events   = EPOLLIN;
	if (epoll_ctl(epoll_file_descriptor, EPOLL_CTL_ADD, module->socket_descriptor, &accept_evt) < 0) assert(0);
}

/***************************************
 * Public Methods
 ***************************************/

/**
 * Module Mega Teardown Function
 * Closes the socket and dynamic library, and then frees the module
 * Returns harmlessly if there are outstanding references
 * @param module - the module to teardown
 **/
void
module__free(struct module *module)
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


/**
 * Module Contructor
 * Creates a new module, invokes initialize_tables to initialize the indirect table, adds it to the module DB, and starts
 *listening for HTTP Requests
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
module__new(char *name, char *path, i32 argument_count, u32 stack_size, u32 max_memory, u32 timeout, int port,
             int request_size, int response_size)
{
	struct module *module = (struct module *)malloc(sizeof(struct module));
	if (!module) return NULL;

	memset(module, 0, sizeof(struct module));

	// Load the dynamic library *.so file with lazy function call binding and deep binding
	module->dynamic_library_handle = dlopen(path, RTLD_LAZY | RTLD_DEEPBIND);
	if (module->dynamic_library_handle == NULL) goto dl_open_error;

	// Resolve the symbols in the dynamic library *.so file
	module->main = (mod_main_fn_t)dlsym(module->dynamic_library_handle, MOD_MAIN_FN);
	if (module->main == NULL) goto dl_error;

	module->initialize_globals = (mod_glb_fn_t)dlsym(module->dynamic_library_handle, MOD_GLB_FN);
	if (module->initialize_globals == NULL) goto dl_error;

	module->initialize_memory = (mod_mem_fn_t)dlsym(module->dynamic_library_handle, MOD_MEM_FN);
	if (module->initialize_memory == NULL) goto dl_error;

	module->initialize_tables = (mod_tbl_fn_t)dlsym(module->dynamic_library_handle, MOD_TBL_FN);
	if (module->initialize_tables == NULL) goto dl_error;

	module->initialize_libc = (mod_libc_fn_t)dlsym(module->dynamic_library_handle, MOD_LIBC_FN);
	if (module->initialize_libc == NULL) goto dl_error;

	// Set fields in the module struct
	strncpy(module->name, name, MOD_NAME_MAX);
	strncpy(module->path, path, MOD_PATH_MAX);

	module->argument_count    = argument_count;
	module->stack_size        = round_up_to_page(stack_size == 0 ? WASM_STACK_SIZE : stack_size);
	module->max_memory        = max_memory == 0 ? ((u64)WASM_PAGE_SIZE * WASM_MAX_PAGES) : max_memory;
	module->timeout           = timeout;
	module->socket_descriptor = -1;
	module->port              = port;
	if (request_size == 0) request_size = MOD_REQ_RESP_DEFAULT;
	if (response_size == 0) response_size = MOD_REQ_RESP_DEFAULT;
	module->max_request_size             = request_size;
	module->max_response_size            = response_size;
	module->max_request_or_response_size = round_up_to_page(request_size > response_size ? request_size
	                                                                                     : response_size);

	// module_indirect_table is a thread-local struct
	struct indirect_table_entry *cache_tbl = module_indirect_table;

	// assumption: All modules are created at program start before we enable preemption or enable the execution of
	// any worker threads We are checking that thread-local module_indirect_table is NULL to prove that we aren't
	// yet preempting If we want to be able to do this later, we can possibly defer module__initialize_table until the
	// first invocation
	assert(cache_tbl == NULL);

	// TODO: determine why we have to set the module_indirect_table state before calling table init and then restore
	// the existing value What is the relationship between these things?
	module_indirect_table = module->indirect_table;
	module__initialize_table(module);
	module_indirect_table = cache_tbl;

	// Add the module to the in-memory module DB
	module_database__add(module);

	// Start listening for requests
	module__initialize_as_server(module);

	return module;

dl_error:
	dlclose(module->dynamic_library_handle);

dl_open_error:
	free(module);
	debuglog("%s\n", dlerror());
	return NULL;
}

