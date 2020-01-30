#ifndef SFRT_MODULE_H
#define SFRT_MODULE_H

#include <uv.h>
#include "types.h"

struct module {
	char name[MOD_NAME_MAX];
	char path[MOD_PATH_MAX];

	void *        dl_handle;
	mod_main_fn_t entry_fn;
	mod_glb_fn_t  glb_init_fn;
	mod_mem_fn_t  mem_init_fn;
	mod_tbl_fn_t  tbl_init_fn;
	mod_libc_fn_t libc_init_fn;

	struct indirect_table_entry indirect_table[INDIRECT_TABLE_SIZE];

	i32 nargs;
	u32 stack_size; // a specification?
	u64 max_memory; // perhaps a specification of the module. (max 4GB)
	u32 timeout;    // again part of the module specification.

	u32 refcnt; // ref count how many instances exist here.

	// stand-alone vs serverless
#ifndef STANDALONE
	struct sockaddr_in srvaddr;
	int                srvsock, srvport;
	// unfortunately, using UV for accepting connections is not great!
	// on_connection, to create a new accepted connection, will have to
	// init a tcp handle, which requires a uvloop. cannot use main as
	// rest of the connection is handled in sandboxing threads, with per-core(per-thread) tls data-structures.
	// so, using direct epoll for accepting connections.
	//	uv_handle_t srvuv;
	unsigned long max_req_sz, max_resp_sz, max_rr_sz; // req/resp from http, (resp size including headers!)..
	int           nreqhdrs, nresphdrs;
	char          reqhdrs[HTTP_HEADERS_MAX][HTTP_HEADER_MAXSZ];
	char          rqctype[HTTP_HEADERVAL_MAXSZ];
	char          rspctype[HTTP_HEADERVAL_MAXSZ];
	char          resphdrs[HTTP_HEADERS_MAX][HTTP_HEADER_MAXSZ];
#endif
};

struct module *module_alloc(char *mod_name, char *mod_path, i32 nargs, u32 stack_sz, u32 max_heap, u32 timeout,
                            int port, int req_sz, int resp_sz);
// frees only if refcnt == 0
void           module_free(struct module *mod);
struct module *module_find_by_name(char *name);
struct module *module_find_by_sock(int sock);

static inline void
module_http_info(struct module *m, int nrq, char *rqs, char rqtype[], int nrs, char *rs, char rsptype[])
{
#ifndef STANDALONE
	assert(m);
	m->nreqhdrs  = nrq;
	m->nresphdrs = nrs;
	memcpy(m->reqhdrs, rqs, HTTP_HEADER_MAXSZ * HTTP_HEADERS_MAX);
	memcpy(m->resphdrs, rs, HTTP_HEADER_MAXSZ * HTTP_HEADERS_MAX);
	strcpy(m->rqctype, rqtype);
	strcpy(m->rspctype, rsptype);
#endif
}

static inline int
module_is_valid(struct module *mod)
{
	if (mod && mod->dl_handle && mod->entry_fn) return 1;

	return 0;
}

static inline void
module_globals_init(struct module *mod)
{
	// called in a sandbox.
	mod->glb_init_fn();
}

static inline void
module_table_init(struct module *mod)
{
	// called at module creation time (once only per module).
	mod->tbl_init_fn();
}

static inline void
module_libc_init(struct module *mod, i32 env, i32 args)
{
	// called in a sandbox.
	mod->libc_init_fn(env, args);
}

static inline void
module_memory_init(struct module *mod)
{
	// called in a sandbox.
	mod->mem_init_fn();
}

static inline i32
module_entry(struct module *mod, i32 argc, i32 argv)
{
	return mod->entry_fn(argc, argv);
}

// instantiate this module.
static inline void
module_acquire(struct module *mod)
{
	// FIXME: atomic.
	mod->refcnt++;
}

static inline void
module_release(struct module *mod)
{
	// FIXME: atomic.
	mod->refcnt--;
}

static inline i32
module_nargs(struct module *mod)
{
	return mod->nargs;
}

#endif /* SFRT_MODULE_H */
