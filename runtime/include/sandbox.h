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
	int                 fd;
	struct stat         s_cache;
	union uv_any_handle uvh;
};

typedef enum
{
	SANDBOX_FREE,
	SANDBOX_RUNNABLE,
	SANDBOX_BLOCKED,
	SANDBOX_WOKEN,    // for race in block()/wakeup()
	SANDBOX_RETURNED, // waiting for parent to read status?
} sandbox_state_t;

/*
 * This is the slowpath switch to a preempted sandbox!
 * SIGUSR1 on the current thread and restore mcontext there!
 */
extern void __attribute__((noreturn)) sandbox_switch_preempt(void);

struct sandbox {
	sandbox_state_t state;

	void *linear_start; // after sandbox struct
	u32   linear_size;  // from after sandbox struct
	u32   linear_max_size;
	u32   sandbox_size;

	void *stack_start; // guess we need a mechanism for stack allocation.
	u32   stack_size;  // and to set the size of it.

	arch_context_t ctxt; // register context for context switch.

	// TODO: are all these necessary?
	u64 actual_deadline;
	u64 expected_deadline;
	u64 total_time;
	u64 remaining_time;
	u64 start_time;

	struct module *module; // which module is this an instance of?

	i32   args_offset; // actual placement of args in the sandbox.
	void *args;        // args from request, must be of module->argument_count size.
	i32   retval;

	struct io_handle handles[SBOX_MAX_OPEN];
	struct sockaddr      client; // client requesting connection!
	int                  csock;
	uv_tcp_t             cuv;
	uv_shutdown_t        cuvsr;
	http_parser          http_parser;
	struct http_request  http_request;
	struct http_response http_response;

	char *  read_buf;
	ssize_t read_len, read_size;

	struct ps_list list;

	ssize_t rr_data_len;      // <= max(module->max_request_or_response_size)
	char    req_resp_data[1]; // of rr_data_sz, following sandbox mem..
} PAGE_ALIGNED;

struct sandbox_request {
	struct module *  module;
	char *           args;
	int              sock;
	struct sockaddr *addr;
	u64 start_time; // cycles
};
typedef struct sandbox_request sbox_request_t;

DEQUE_PROTOTYPE(sandbox, sbox_request_t *);

static inline int sandbox_deque_push(sbox_request_t *sandbox_request);

// a runtime resource, malloc on this!
struct sandbox *sandbox_alloc(struct module *module, char *args, int sock, const struct sockaddr *addr, u64 start_time);
// should free stack and heap resources.. also any I/O handles.
void sandbox_free(struct sandbox *sandbox);

extern __thread struct sandbox *current_sandbox;
// next_sandbox only used in SIGUSR1
extern __thread arch_context_t *next_context;

typedef struct sandbox sandbox_t;

/**
 * Allocates a new Sandbox Request and places it on the Global Deque
 * @param module the module we want to request
 * @param args the arguments that we'll pass to the serverless function
 * @param sock
 * @param addr
 * @param start_time the timestamp of when we receives the request from the network (in cycles)
 * @return the new sandbox request
 **/
static inline sbox_request_t *
sbox_request_alloc(
	struct module *module, 
	char *args, 
	int sock, 
	const struct sockaddr *addr, 
	u64 start_time)
{
	sbox_request_t *sandbox_request = malloc(sizeof(sbox_request_t));
	assert(sandbox_request);
	sandbox_request->module  = module;
	sandbox_request->args = args;
	sandbox_request->sock = sock;
	sandbox_request->addr = (struct sockaddr *)addr;
	sandbox_request->start_time = start_time;

	debuglog("[%p: %s]\n", sandbox_request, sandbox_request->module->name);
	sandbox_deque_push(sandbox_request);
	return sandbox_request;
}

/**
 * Getter for the current sandbox executing on this thread
 * @returns the current sandbox executing on this thread
 **/
static inline struct sandbox *
sandbox_current(void)
{
	return current_sandbox;
}

/**
 * Setter for the current sandbox executing on this thread
 * @param sandbox the sandbox we are setting this thread to run
 **/
static inline void
sandbox_current_set(struct sandbox *sandbox)
{
	// FIXME: critical-section.
	current_sandbox = sandbox;
	if (sandbox == NULL) return;

	// Thread Local State about the Current Sandbox
	sandbox_lmbase  = sandbox->linear_start;
	sandbox_lmbound = sandbox->linear_size;
	// TODO: module table or sandbox table?
	module_indirect_table = sandbox->module->indirect_table;
}

/**
 * Check that the current_sandbox struct matches the rest of the thread local state about the executing sandbox
 * This includes lmbase, lmbound, and module_indirect_table
 */
static inline void
sandbox_current_check(void)
{
	struct sandbox *sandbox = sandbox_current();
	assert(sandbox && sandbox->linear_start == sandbox_lmbase && sandbox->linear_size == sandbox_lmbound);
	assert(sandbox->module->indirect_table == module_indirect_table);
}

/**
 * Given a sandbox, returns the module that sandbox is executing
 * @param sandbox the sandbox whose module we want
 * @return the module of the provided sandbox
 */
static inline struct module *
sandbox_module(struct sandbox *sandbox)
{
	if (!sandbox) return NULL;
	return sandbox->module;
}

extern void sandbox_local_end(struct sandbox *sandbox);

/**
 * @brief Switches to the next sandbox, placing the current sandbox of the completion queue if in RETURNED state
 * @param next The Sandbox Context to switch to or NULL
 * @return void
 */
static inline void
sandbox_switch(struct sandbox *next_sandbox)
{
	arch_context_t *next_register_context = next_sandbox == NULL ? NULL : &next_sandbox->ctxt;
	softint_disable();
	struct sandbox *current_sandbox = sandbox_current();
	arch_context_t *current_register_context = current_sandbox == NULL ? NULL : &current_sandbox->ctxt;
	sandbox_current_set(next_sandbox);
	// If the current sandbox we're switching from is in a RETURNED state, add to completion queue
	if (current_sandbox && current_sandbox->state == SANDBOX_RETURNED) sandbox_local_end(current_sandbox);
	next_context = next_register_context;
	arch_context_switch(current_register_context, next_register_context);
	softint_enable();
}

/**
 * Getter for the arguments of the current sandbox
 * @return the arguments of the current sandbox
 */
static inline char *
sandbox_args(void)
{
	struct sandbox *sandbox = sandbox_current();
	return (char *)sandbox->args;
}

void *          sandbox_run_func(void *data);
struct sandbox *sandbox_schedule(int interrupt);
void            sandbox_block(void);
void            sandbox_wakeup(sandbox_t *sb);
// called in sandbox_entry() before and after fn() execution
// for http request/response processing using uvio
void sandbox_block_http(void);
void sandbox_response(void);

// should be the entry-point for each sandbox so it can do per-sandbox mem/etc init.
// should have been called with stack allocated and sandbox_current() set!
void                         sandbox_entry(void);
void                         sandbox_exit(void);
extern struct deque_sandbox *global_deque;
extern pthread_mutex_t       global_deque_mutex;


/**
 * Pushes a sandbox request to the global deque
 * @para 
 **/
static inline int
sandbox_deque_push(sbox_request_t *sandbox_request)
{
	int return_code;

#if NCORES == 1
	pthread_mutex_lock(&global_deque_mutex);
#endif
	return_code = deque_push_sandbox(global_deque, &sandbox_request);
#if NCORES == 1
	pthread_mutex_unlock(&global_deque_mutex);
#endif

	return return_code;
}

static inline int
sandbox_deque_pop(sbox_request_t **sandbox_request)
{
	int return_code;

#if NCORES == 1
	pthread_mutex_lock(&global_deque_mutex);
#endif
	return_code = deque_pop_sandbox(global_deque, sandbox_request);
#if NCORES == 1
	pthread_mutex_unlock(&global_deque_mutex);
#endif

	return return_code;
}

/**
 * @returns A Sandbox Request or NULL
 **/
static inline sbox_request_t *
sandbox_deque_steal(void)
{
	sbox_request_t *sandbox_request = NULL;

#if NCORES == 1
	sandbox_deque_pop(&sandbox_request);
#else
	// TODO: check! is there a sandboxing thread on same core as udp-server thread?
	int r = deque_steal_sandbox(global_deque, &sandbox_request);
	if (r) sandbox_request = NULL;
#endif

	return sandbox_request;
}

static inline int
io_handle_preopen(void)
{
	struct sandbox *sandbox = sandbox_current();
	int             i;
	for (i = 0; i < SBOX_MAX_OPEN; i++) {
		if (sandbox->handles[i].fd < 0) break;
	}
	if (i == SBOX_MAX_OPEN) return -1;
	sandbox->handles[i].fd = SBOX_PREOPEN_MAGIC;
	memset(&sandbox->handles[i].uvh, 0, sizeof(union uv_any_handle));
	return i;
}

static inline int
io_handle_open(int fd)
{
	struct sandbox *sandbox = sandbox_current();
	if (fd < 0) return fd;
	int i            = io_handle_preopen();
	sandbox->handles[i].fd = fd; // well, per sandbox.. so synchronization necessary!
	return i;
}

/**
 * Sets the file descriptor of the sandbox's ith io_handle
 * Returns error condition if the fd to set does not contain sandbox preopen magin
 * @param idx index of the sandbox handles we want to set
 * @param fd the file descripter we want to set it to
 * @returns the idx that was set or -1 in case of error
 **/
static inline int
io_handle_preopen_set(int idx, int fd)
{
	struct sandbox *sandbox = sandbox_current();
	if (idx >= SBOX_MAX_OPEN || idx < 0) return -1;
	if (fd < 0 || sandbox->handles[idx].fd != SBOX_PREOPEN_MAGIC) return -1;
	sandbox->handles[idx].fd = fd;
	return idx;
}

/**
 * Get the file descriptor of the sandbox's ith io_handle
 * @param idx index into the sandbox's handles table
 * @returns any libuv handle 
 **/
static inline int
io_handle_fd(int idx)
{
	struct sandbox *sandbox = sandbox_current();
	if (idx >= SBOX_MAX_OPEN || idx < 0) return -1;
	return sandbox->handles[idx].fd;
}

/**
 * Close the sandbox's ith io_handle 
 * @param idx index of the handle to close
 **/
static inline void
io_handle_close(int idx)
{
	struct sandbox *sandbox = sandbox_current();
	if (idx >= SBOX_MAX_OPEN || idx < 0) return;
	sandbox->handles[idx].fd = -1;
}

/**
 * Get the Libuv handle located at idx of the sandbox ith io_handle 
 * @param idx index of the handle containing uvh???
 * @returns any libuv handle 
 **/
static inline union uv_any_handle *
io_handle_uv_get(int idx)
{
	struct sandbox *sandbox = sandbox_current();
	if (idx >= SBOX_MAX_OPEN || idx < 0) return NULL;
	return &sandbox->handles[idx].uvh;
}

#endif /* SFRT_SANDBOX_H */
