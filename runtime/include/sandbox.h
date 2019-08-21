#ifndef SFRT_SANDBOX_H
#define SFRT_SANDBOX_H

#include "ps_list.h"
#include "module.h"
#include "arch/context.h"
#include "softint.h"
#include <ucontext.h>
#include <uv.h>

struct io_handle {
	int fd;
	struct stat s_cache;
	union uv_any_handle uvh;
};

typedef enum {
	SANDBOX_FREE,
	SANDBOX_RUNNABLE,
	SANDBOX_BLOCKED,
	SANDBOX_WOKEN, //for race in block()/wakeup()
	SANDBOX_RETURNED, //waiting for parent to read status?
} sandbox_state_t;

/*
 * This is the slowpath switch to a preempted sandbox!
 * SIGUSR1 on the current thread and restore mcontext there!
 */
extern void __attribute__((noreturn)) sandbox_switch_preempt(void);

struct sandbox {
	sandbox_state_t state;

	void *linear_start;
	u32   linear_size;
	u32   linear_max_size;

	void *stack_start; // guess we need a mechanism for stack allocation.
	u32   stack_size;  // and to set the size of it.

	arch_context_t ctxt; //register context for context switch.

	// TODO: are all these necessary? 
	u64 actual_deadline;
	u64 expected_deadline;
	u64 total_time;
	u64 remaining_time;
	u64 start_time;

	struct module *mod; //which module is this an instance of?
	//struct indirect_table_entry indirect_table[INDIRECT_TABLE_SIZE];

	i32 args_offset; //actual placement of args in the sandbox.
	/* i32 ret_offset; //placement of return value(s) in the sandbox. */
	void *args; // args from request, must be of module->nargs size.
	i32 retval;

	struct io_handle handles[SBOX_MAX_OPEN];

	char *read_buf;
	ssize_t read_len, read_size;

	struct ps_list list;

	// track I/O handles?
};

// a runtime resource, malloc on this!
struct sandbox *sandbox_alloc(struct module *mod, char *args);
// should free stack and heap resources.. also any I/O handles.
void sandbox_free(struct sandbox *sbox);

// next_sandbox only used in SIGUSR1
extern __thread struct sandbox *current_sandbox;
extern __thread arch_context_t *next_context;

typedef struct sandbox sandbox_t;

static inline struct sandbox *
sandbox_current(void)
{ return current_sandbox; }

static inline void
sandbox_current_set(struct sandbox *sbox)
{
	int dis = 0;

	if (softint_enabled()) {
		dis = 1;
		softint_disable();
	}

	// FIXME: critical-section. 
	current_sandbox = sbox;
	if (sbox == NULL) return;

	sandbox_lmbase = sbox->linear_start;
	sandbox_lmbound = sbox->linear_size;
	// TODO: module table or sandbox table?
	module_indirect_table = sbox->mod->indirect_table;

	if (dis) softint_enable();
}

static inline struct module *
sandbox_module(struct sandbox *s)
{
	if (!s) return NULL;

	return s->mod;
}

static inline void
sandbox_switch(struct sandbox *next)
{
	arch_context_t *n = next == NULL ? NULL : &next->ctxt;

	// disable interrupts (signals)
	softint_disable();

	// switch sandbox (register context & base/bound/table)
	struct sandbox *curr = sandbox_current();
	arch_context_t *c = curr == NULL ? NULL : &curr->ctxt;
	sandbox_current_set(next);
	// save current's registers and restore next's registers.
	next_context = n;
	arch_context_switch(c, n);
	next_context = NULL;

	// enable interrupts (signals)
	softint_enable();
}

static inline char *
sandbox_args(void)
{
	struct sandbox *c = sandbox_current();

	return (char *)c->args;
}

void sandbox_run(struct sandbox *s);
void *sandbox_run_func(void *data);
struct sandbox *sandbox_schedule(void);
void sandbox_block(void);
void sandbox_wakeup(sandbox_t *sb);

// should be the entry-point for each sandbox so it can do per-sandbox mem/etc init.
// should have been called with stack allocated and sandbox_current() set!
void sandbox_entry(void);
void sandbox_exit(void);

static inline int
io_handle_preopen(void)
{
	struct sandbox *s = sandbox_current();
	int i;
	for (i = 0; i < SBOX_MAX_OPEN; i++) {
		if (s->handles[i].fd < 0) break;
	}
	if (i == SBOX_MAX_OPEN) return -1;
	s->handles[i].fd = SBOX_PREOPEN_MAGIC;
	memset(&s->handles[i].uvh, 0, sizeof(union uv_any_handle));
	return i;
}

static inline int
io_handle_open(int fd)
{
	struct sandbox *s = sandbox_current();
	if (fd < 0) return fd;
	int i = io_handle_preopen();
	s->handles[i].fd = fd; //well, per sandbox.. so synchronization necessary!
	return i;
}

static inline int
io_handle_preopen_set(int idx, int fd)
{
	struct sandbox *s = sandbox_current();
	if (idx >= SBOX_MAX_OPEN || idx < 0) return -1;
	if (fd < 0 || s->handles[idx].fd != SBOX_PREOPEN_MAGIC) return -1;
	s->handles[idx].fd = fd;
	return idx;
}

static inline int
io_handle_fd(int idx)
{
	struct sandbox *s = sandbox_current();
	if (idx >= SBOX_MAX_OPEN || idx < 0) return -1;
	return s->handles[idx].fd;
}

static inline void
io_handle_close(int idx)
{
	struct sandbox *s = sandbox_current();
	if (idx >= SBOX_MAX_OPEN || idx < 0) return;
	s->handles[idx].fd = -1;
}

static inline union uv_any_handle *
io_handle_uv_get(int idx)
{
	struct sandbox *s = sandbox_current();
	if (idx >= SBOX_MAX_OPEN || idx < 0) return NULL;
	return &s->handles[idx].uvh;
}

#endif /* SFRT_SANDBOX_H */
