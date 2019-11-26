#include <runtime.h>
#include <module.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <uv.h>
#include <util.h>

static struct module *__mod_db[MOD_MAX] = { NULL };
static int __mod_free_off = 0;

// todo: optimize this.. do we care? plus not atomic!! 
struct module *
module_find(char *name)
{
	int f = __mod_free_off;
	for (int i = 0; i < f; i++) {
		assert(__mod_db[i]);
		if (strcmp(__mod_db[i]->name, name) == 0) return __mod_db[i];
	}
	return NULL;
}

static inline int
module_add(struct module *m)
{
	assert(module_find(m->name) == NULL);

	int f = __sync_fetch_and_add(&__mod_free_off, 1);
	assert(f < MOD_MAX);
	__mod_db[f] = m;
	return 0;
}

static void
module_on_recv(uv_udp_t *h, ssize_t nr, const uv_buf_t *rcvbuf, const struct sockaddr *addr, unsigned flags)
{
	if (nr <= 0) goto done;

	debuglog("MC:%s, %s\n", h->data, rcvbuf->base);
	// invoke a function!
	struct sandbox *s = util_parse_sandbox_string_json((struct module *)(h->data), rcvbuf->base);
	//struct sandbox *s = util_parse_sandbox_string_custom((struct module *)(h->data), rcvbuf->base);
	assert(s);

done:
	free(rcvbuf->base);
}

static void
module_io_init(struct module *m)
{
	int status;

	status = uv_udp_init(uv_default_loop(), &m->udpsrv);
	assert(status >= 0);

	debuglog("MIO:%s,%u\n", m->name, m->udpport);
	uv_ip4_addr("127.0.0.1", m->udpport, &m->srvaddr);
	status = uv_udp_bind(&m->udpsrv, (const struct sockaddr *)&m->srvaddr, 0);
	assert(status >= 0);
	m->udpsrv.data = (void *)m;

	status = uv_udp_recv_start(&m->udpsrv, runtime_on_alloc, module_on_recv);
	assert(status >= 0);
}

struct module *
module_alloc(char *modname, char *modpath, u32 udp_port, i32 nargs, i32 nrets, u32 stacksz, u32 maxheap, u32 timeout)
{
	// FIXME: cannot do this at runtime, we may be interfering with a sandbox's heap!
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
	/* mod->nrets = nrets; */
	mod->stack_size = stacksz == 0 ? WASM_STACK_SIZE : stacksz;
	mod->max_memory = maxheap == 0 ? ((u64)WASM_PAGE_SIZE * WASM_MAX_PAGES) : maxheap;
	mod->timeout    = timeout;

	struct indirect_table_entry *cache_tbl = module_indirect_table;
	// assumption: modules are created before enabling preemption and before running runtime-sandboxing threads..
	// if this isn't a good assumption, just let the first invocation do table init..!!
	assert(cache_tbl == NULL);
	module_indirect_table = mod->indirect_table;
	module_table_init(mod);
	module_indirect_table = cache_tbl;
	mod->udpport = udp_port;
	module_add(mod);
#ifndef STANDALONE
	module_io_init(mod);
#endif

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

	dlclose(mod->dl_handle);
	memset(mod, 0, sizeof(struct module));

	// FIXME: use global/static memory. cannot interfere with some sandbox's heap!
	free(mod);
}
