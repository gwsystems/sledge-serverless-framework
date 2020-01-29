#ifndef SFRT_SANDBOX_H
#define SFRT_SANDBOX_H

#include "ps_list.h"
#include "module.h"
#include "arch/context.h"
#include "softint.h"
#include <ucontext.h>
#include <uv.h>
#include "deque.h"
#include <http.h>

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

	void *linear_start; // after sandbox struct
	u32   linear_size; // from after sandbox struct
	u32   linear_max_size;
	u32   sb_size;

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

	i32 args_offset; //actual placement of args in the sandbox.
	void *args; // args from request, must be of module->nargs size.
	i32 retval;

	struct io_handle handles[SBOX_MAX_OPEN];
#ifndef STANDALONE
	struct sockaddr client; //client requesting connection!
	int csock;
	uv_tcp_t cuv;
	uv_shutdown_t cuvsr;
	http_parser hp;
	struct http_request rqi;
	struct http_response rsi;
#endif

	char *read_buf;
	ssize_t read_len, read_size;

	struct ps_list list;

	ssize_t  rr_data_len; // <= max(mod->max_rr_sz)
	char req_resp_data[1]; //of rr_data_sz, following sandbox mem..
} PAGE_ALIGNED;

#ifndef STANDALONE
#ifdef SBOX_SCALE_ALLOC
struct sandbox_request {
	struct module *mod;
	char *args;
	int sock;
	struct sockaddr *addr;
};
typedef struct sandbox_request sbox_request_t;
#else
typedef struct sandbox sbox_request_t;
#endif
#else
typedef struct sandbox sbox_request_t;
#endif

DEQUE_PROTOTYPE(sandbox, sbox_request_t *);

// a runtime resource, malloc on this!
struct sandbox *sandbox_alloc(struct module *mod, char *args, int sock, const struct sockaddr *addr);
// should free stack and heap resources.. also any I/O handles.
void sandbox_free(struct sandbox *sbox);

extern __thread struct sandbox *current_sandbox;
// next_sandbox only used in SIGUSR1
extern __thread arch_context_t *next_context;

typedef struct sandbox sandbox_t;
void sandbox_run(sbox_request_t *s);

static inline sbox_request_t *
sbox_request_alloc(struct module *mod, char *args, int sock, const struct sockaddr *addr)
{
#ifndef STANDALONE
#ifdef SBOX_SCALE_ALLOC
	sbox_request_t *s = malloc(sizeof(sbox_request_t));
	assert(s);
	s->mod = mod;
	s->args = args;
	s->sock = sock;
	s->addr = (struct sockaddr *)addr;
	sandbox_run(s);
	return s;
#else
	return sandbox_alloc(mod, args, sock, addr);
#endif
#else
	return sandbox_alloc(mod, args, sock, addr);
#endif
}

static inline struct sandbox *
sandbox_current(void)
{ return current_sandbox; }

static inline void
sandbox_current_set(struct sandbox *sbox)
{
	// FIXME: critical-section. 
	current_sandbox = sbox;
	if (sbox == NULL) return;

	sandbox_lmbase = sbox->linear_start;
	sandbox_lmbound = sbox->linear_size;
	// TODO: module table or sandbox table?
	module_indirect_table = sbox->mod->indirect_table;
}

static inline void
sandbox_current_check(void)
{
	struct sandbox *c = sandbox_current();

	assert(c && c->linear_start == sandbox_lmbase && c->linear_size == sandbox_lmbound);
	assert(c->mod->indirect_table == module_indirect_table);
}

static inline struct module *
sandbox_module(struct sandbox *s)
{
	if (!s) return NULL;

	return s->mod;
}

extern void sandbox_local_end(struct sandbox *s);
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
	if (curr && curr->state == SANDBOX_RETURNED) sandbox_local_end(curr);
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

//void sandbox_run(struct sandbox *s);
void *sandbox_run_func(void *data);
struct sandbox *sandbox_schedule(int interrupt);
void sandbox_block(void);
void sandbox_wakeup(sandbox_t *sb);
// called in sandbox_entry() before and after fn() execution 
// for http request/response processing using uvio
void sandbox_block_http(void);
void sandbox_response(void);

// should be the entry-point for each sandbox so it can do per-sandbox mem/etc init.
// should have been called with stack allocated and sandbox_current() set!
void sandbox_entry(void);
void sandbox_exit(void);
extern struct deque_sandbox *glb_dq;
extern pthread_mutex_t glbq_mtx; 

static inline int
sandbox_deque_push(sbox_request_t *s)
{
	int ret;

#if NCORES == 1
	pthread_mutex_lock(&glbq_mtx);
#endif
	ret = deque_push_sandbox(glb_dq, &s);
#if NCORES == 1
	pthread_mutex_unlock(&glbq_mtx);
#endif

	return ret;
}

static inline int
sandbox_deque_pop(sbox_request_t **s)
{
	int ret;

#if NCORES == 1
	pthread_mutex_lock(&glbq_mtx);
#endif
	ret = deque_pop_sandbox(glb_dq, s);
#if NCORES == 1
	pthread_mutex_unlock(&glbq_mtx);
#endif

	return ret;
}

static inline sbox_request_t *
sandbox_deque_steal(void)
{
	sbox_request_t *s = NULL;

#if NCORES == 1
	sandbox_deque_pop(&s);
#else
	// TODO: check! is there a sandboxing thread on same core as udp-server thread?
	int r = deque_steal_sandbox(glb_dq, &s);
	if (r) s = NULL;
#endif

	return s;
}

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
