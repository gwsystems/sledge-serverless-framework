#ifndef SFRT_MODULE_H
#define SFRT_MODULE_H

#include <uv.h>
#include "types.h"

struct module {
	char name[MOD_NAME_MAX]; //not sure if i care for now.
	char path[MOD_PATH_MAX]; //to dlopen if it has not been opened already.

	void *dl_handle;

	mod_main_fn_t entry_fn;
	mod_glb_fn_t glb_init_fn;
	mod_mem_fn_t mem_init_fn;
	mod_tbl_fn_t tbl_init_fn;

	i32 nargs; //as per the specification somewhere.
	/* i32 nrets; */

	u32 stack_size; // a specification?
	u64 max_memory; //perhaps a specification of the module.
	u32 timeout; //again part of the module specification.

	u32 refcnt; //ref count how many instances exist here.

	u32 udpport;
	uv_udp_t udpsrv; // udp server to listen to requests.
	struct sockaddr_in srvaddr;

	// FIXME: for now, per-sandbox. no reason to have it be per-sandbox.
	struct indirect_table_entry indirect_table[INDIRECT_TABLE_SIZE];
	// TODO: what else? 
};

// a runtime resource, perhaps use "malloc" on this? 
struct module *module_alloc(char *mod_name, char *mod_path, u32 udp_port, i32 nargs, i32 nrets, u32 stack_sz, u32 max_heap, u32 timeout/*, ...*/);
// frees only if refcnt == 0
void module_free(struct module *mod);
struct module *module_find(char *name);

static inline int
module_is_valid(struct module *mod)
{
	if (mod && mod->dl_handle && mod->entry_fn) return 1;

	return 0;
}

static inline void
module_table_init(struct module *mod)
{
	// called in a sandbox.
	mod->tbl_init_fn();
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
