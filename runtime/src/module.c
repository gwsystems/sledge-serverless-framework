#include <runtime.h>
#include <module.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <uv.h>
#include <util.h>

static struct module *__mod_db[MOD_MAX] = { NULL };
static int            __mod_free_off    = 0;

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

struct module *
module_find_by_sock(int sock)
{
	int f = __mod_free_off;
	for (int i = 0; i < f; i++) {
		assert(__mod_db[i]);
		if (__mod_db[i]->srvsock == sock) return __mod_db[i];
	}
	return NULL;
}

static inline int
module_add(struct module *module)
{
	assert(module->srvsock == -1);

	int f = __sync_fetch_and_add(&__mod_free_off, 1);
	assert(f < MOD_MAX);
	__mod_db[f] = module;

	return 0;
}

static inline void
module_server_init(struct module *module)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd > 0);
	module->srvaddr.sin_family      = AF_INET;
	module->srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	module->srvaddr.sin_port        = htons((unsigned short)module->srvport);

	int optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (bind(fd, (struct sockaddr *)&module->srvaddr, sizeof(module->srvaddr)) < 0) {
		perror("bind");
		assert(0);
	}
	if (listen(fd, MOD_BACKLOG) < 0) assert(0);
	module->srvsock = fd;

	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)module;
	accept_evt.events   = EPOLLIN;

	if (epoll_ctl(epoll_file_descriptor, EPOLL_CTL_ADD, module->srvsock, &accept_evt) < 0) assert(0);
}

struct module *
module_alloc(char *module_name, char *module_path, i32 argument_count, u32 stack_size, u32 max_memory, u32 timeout, int port, int request_size,
             int response_size)
{
	struct module *module = (struct module *)malloc(sizeof(struct module));
	if (!module) return NULL;

	memset(module, 0, sizeof(struct module));
	module->dl_handle = dlopen(module_path, RTLD_LAZY | RTLD_DEEPBIND);
	if (module->dl_handle == NULL) goto dl_open_error;

	module->entry_fn = (mod_main_fn_t)dlsym(module->dl_handle, MOD_MAIN_FN);
	if (module->entry_fn == NULL) goto dl_error;

	module->glb_init_fn = (mod_glb_fn_t)dlsym(module->dl_handle, MOD_GLB_FN);
	if (module->glb_init_fn == NULL) goto dl_error;

	module->mem_init_fn = (mod_mem_fn_t)dlsym(module->dl_handle, MOD_MEM_FN);
	if (module->mem_init_fn == NULL) goto dl_error;

	module->tbl_init_fn = (mod_tbl_fn_t)dlsym(module->dl_handle, MOD_TBL_FN);
	if (module->tbl_init_fn == NULL) goto dl_error;

	module->libc_init_fn = (mod_libc_fn_t)dlsym(module->dl_handle, MOD_LIBC_FN);
	if (module->libc_init_fn == NULL) goto dl_error;

	strncpy(module->name, module_name, MOD_NAME_MAX);
	strncpy(module->path, module_path, MOD_PATH_MAX);

	module->argument_count      = argument_count;
	module->stack_size = round_up_to_page(stack_size == 0 ? WASM_STACK_SIZE : stack_size);
	module->max_memory = max_memory == 0 ? ((u64)WASM_PAGE_SIZE * WASM_MAX_PAGES) : max_memory;
	module->timeout    = timeout;
	module->srvsock = -1;
	module->srvport = port;
	if (request_size == 0) request_size = MOD_REQ_RESP_DEFAULT;
	if (response_size == 0) response_size = MOD_REQ_RESP_DEFAULT;
	module->max_request_size  = request_size;
	module->max_response_size = response_size;
	module->max_request_or_response_size   = round_up_to_page(request_size > response_size ? request_size : response_size);

	struct indirect_table_entry *cache_tbl = module_indirect_table;
	// assumption: modules are created before enabling preemption and before running runtime-sandboxing threads..
	// if this isn't a good assumption, just let the first invocation do table init..!!
	assert(cache_tbl == NULL);
	module_indirect_table = module->indirect_table;
	module_table_init(module);
	module_indirect_table = cache_tbl;
	module_add(module);
	module_server_init(module);

	return module;

dl_error:
	dlclose(module->dl_handle);

dl_open_error:
	free(module);
	debuglog("%s\n", dlerror());

	return NULL;
}

void
module_free(struct module *module)
{
	if (module == NULL) return;
	if (module->dl_handle == NULL) return;

	// Do not free if we still have oustanding references
	if (module->reference_count) return;

	close(module->srvsock);
	dlclose(module->dl_handle);
	free(module);
}
