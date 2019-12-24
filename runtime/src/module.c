#include <runtime.h>
#include <module.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <uv.h>
#include <util.h>

static struct module *__mod_db[MOD_MAX] = { NULL };
static int __mod_free_off = 0;

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
#ifndef STANDALONE
	int f = __mod_free_off;
	for (int i = 0; i < f; i++) {
		assert(__mod_db[i]);
		if (__mod_db[i]->srvsock == sock) return __mod_db[i];
	}
#endif
	return NULL;
}

static inline int
module_add(struct module *m)
{
#ifdef STANDALONE
	assert(module_find_by_name(m->name) == NULL);
#else
	assert(m->srvsock == -1);
#endif

	int f = __sync_fetch_and_add(&__mod_free_off, 1);
	assert(f < MOD_MAX);
	__mod_db[f] = m;

	return 0;
}

static inline void
module_server_init(struct module *m)
{
#ifndef STANDALONE
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd > 0);
	m->srvaddr.sin_family = AF_INET;
	m->srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	m->srvaddr.sin_port = htons((unsigned short)m->srvport);

	int optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
	optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if (bind(fd, (struct sockaddr *)&m->srvaddr, sizeof(m->srvaddr)) < 0) {
		perror("bind");
		assert(0);
	}
	if (listen(fd, MOD_BACKLOG) < 0) assert(0);
	m->srvsock = fd;

	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)m;
	accept_evt.events   = EPOLLIN;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, m->srvsock, &accept_evt) < 0) assert(0);
#endif
}

struct module *
module_alloc(char *modname, char *modpath, i32 nargs, u32 stacksz, u32 maxheap, u32 timeout, int port, int req_sz, int resp_sz)
{
	struct module *mod = (struct module *)malloc(sizeof(struct module));
	if (!mod) return NULL;

	memset(mod, 0, sizeof(struct module));
	mod->dl_handle = dlopen(modpath, RTLD_LAZY | RTLD_DEEPBIND);
	if (mod->dl_handle == NULL) goto dl_open_error;

	mod->entry_fn = (mod_main_fn_t)dlsym(mod->dl_handle, MOD_MAIN_FN);
	if (mod->entry_fn == NULL) goto dl_error;
	
	// TODO: don't think this is necessary or implemented.
	//mod->glb_init_fn = (mod_glb_fn_t)dlsym(mod->dl_handle, MOD_GLB_FN);
	//if (mod->glb_init_fn == NULL) goto dl_error;

	mod->mem_init_fn = (mod_mem_fn_t)dlsym(mod->dl_handle, MOD_MEM_FN);
	if (mod->mem_init_fn == NULL) goto dl_error;

	mod->tbl_init_fn = (mod_tbl_fn_t)dlsym(mod->dl_handle, MOD_TBL_FN);
	if (mod->tbl_init_fn == NULL) goto dl_error;

	strncpy(mod->name, modname, MOD_NAME_MAX);
	strncpy(mod->path, modpath, MOD_PATH_MAX);

	mod->nargs = nargs;	
	mod->stack_size = round_up_to_page(stacksz == 0 ? WASM_STACK_SIZE : stacksz);
	mod->max_memory = maxheap == 0 ? ((u64)WASM_PAGE_SIZE * WASM_MAX_PAGES) : maxheap;
	mod->timeout    = timeout;
#ifndef STANDALONE
	mod->srvsock = -1;
	mod->srvport = port;
	if (req_sz == 0) req_sz = MOD_REQ_RESP_DEFAULT;
	if (resp_sz == 0) resp_sz = MOD_REQ_RESP_DEFAULT;
	mod->max_req_sz = req_sz;
	mod->max_resp_sz = resp_sz;
	mod->max_rr_sz = round_up_to_page(req_sz > resp_sz ? req_sz : resp_sz);
#endif

	struct indirect_table_entry *cache_tbl = module_indirect_table;
	// assumption: modules are created before enabling preemption and before running runtime-sandboxing threads..
	// if this isn't a good assumption, just let the first invocation do table init..!!
	assert(cache_tbl == NULL);
	module_indirect_table = mod->indirect_table;
	module_table_init(mod);
	module_indirect_table = cache_tbl;
	module_add(mod);
	module_server_init(mod);

	return mod;

dl_error:
	dlclose(mod->dl_handle);

dl_open_error:
	free(mod);
	debuglog("%s\n", dlerror());

	return NULL;
}

void
module_free(struct module *mod)
{
	if (mod == NULL) return;
	if (mod->dl_handle == NULL) return;
	if (mod->refcnt) return;

#ifndef STANDALONE
	close(mod->srvsock);
#endif
	dlclose(mod->dl_handle);
	free(mod);
}
