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
	int                 file_descriptor;
	union uv_any_handle libuv_handle;
};

typedef enum
{
	RUNNABLE,
	BLOCKED,
	RETURNED
} sandbox_state_t;

/*
 * This is the slowpath switch to a preempted sandbox!
 * SIGUSR1 on the current thread and restore mcontext there!
 */
extern void __attribute__((noreturn)) sandbox_switch_preempt(void);

// TODO: linear_memory_max_size is not really used

struct sandbox {
	sandbox_state_t state;

	u32   sandbox_size; // The struct plus enough buffer to hold the request or response (sized off largest)
	
	void *linear_memory_start; // after sandbox struct
	u32   linear_memory_size;  // from after sandbox struct
	u32   linear_memory_max_size;

	void *stack_start; // guess we need a mechanism for stack allocation.
	u32   stack_size;  // and to set the size of it.

	arch_context_t ctxt; // register context for context switch.

	// TODO: Refactor to usefully track across scheduler
	u64 actual_deadline;
	u64 expected_deadline;
	u64 total_time;
	u64 remaining_time;
	u64 start_time;

	struct module *module; // the module this is an instance of

	i32   arguments_offset; // actual placement of arguments in the sandbox.
	void *arguments;        // arguments from request, must be of module->argument_count size.
	i32   return_value;

	struct io_handle     handles[SBOX_MAX_OPEN];
	struct sockaddr      client_address; // client requesting connection!
	int                  client_socket_descriptor;
	uv_tcp_t             client_libuv_stream;
	uv_shutdown_t        client_libuv_shutdown_request;

	http_parser          http_parser;
	struct http_request  http_request;
	struct http_response http_response;

	char *  read_buffer;
	ssize_t read_length, read_size;

	// TODO: Is this used?
	struct ps_list list;

	ssize_t request_response_data_length;      // <= max(module->max_request_or_response_size)
	char    request_response_data[1]; // of rr_data_sz, following sandbox mem..
} PAGE_ALIGNED;

struct sandbox_request {
	struct module *  module;
	char *           arguments;
	int              socket_descriptor;
	struct sockaddr *socket_address;
	u64              start_time; // cycles
};
typedef struct sandbox_request sandbox_request_t;

DEQUE_PROTOTYPE(sandbox, sandbox_request_t *);

static inline int add_sandbox_request_to_global_dequeue(sandbox_request_t *sandbox_request);

// a runtime resource, malloc on this!
struct sandbox *allocate_sandbox(struct module *module, char *arguments, int socket_descriptor, const struct sockaddr *socket_address, u64 start_time);
// should free stack and heap resources.. also any I/O handles.
void free_sandbox(struct sandbox *sandbox);

extern __thread struct sandbox *current_sandbox;
// next_sandbox only used in SIGUSR1
extern __thread arch_context_t *next_context;

typedef struct sandbox sandbox_t;

/**
 * Allocates a new Sandbox Request and places it on the Global Deque
 * @param module the module we want to request
 * @param arguments the arguments that we'll pass to the serverless function
 * @param socket_descriptor
 * @param socket_address
 * @param start_time the timestamp of when we receives the request from the network (in cycles)
 * @return the new sandbox request
 **/
static inline sandbox_request_t *
allocate_sandbox_request(struct module *module, char *arguments, int socket_descriptor, const struct sockaddr *socket_address, u64 start_time)
{
	sandbox_request_t *sandbox_request = malloc(sizeof(sandbox_request_t));
	assert(sandbox_request);
	sandbox_request->module     = module;
	sandbox_request->arguments       = arguments;
	sandbox_request->socket_descriptor       = socket_descriptor;
	sandbox_request->socket_address       = (struct sockaddr *)socket_address;
	sandbox_request->start_time = start_time;

	debuglog("[%p: %s]\n", sandbox_request, sandbox_request->module->name);
	add_sandbox_request_to_global_dequeue(sandbox_request);
	return sandbox_request;
}

/**
 * Getter for the current sandbox executing on this thread
 * @returns the current sandbox executing on this thread
 **/
static inline struct sandbox *
get_current_sandbox(void)
{
	return current_sandbox;
}

/**
 * Setter for the current sandbox executing on this thread
 * @param sandbox the sandbox we are setting this thread to run
 **/
static inline void
set_current_sandbox(struct sandbox *sandbox)
{
	// FIXME: critical-section.
	current_sandbox = sandbox;
	if (sandbox == NULL) return;

	// Thread Local State about the Current Sandbox
	sandbox_lmbase  = sandbox->linear_memory_start;
	sandbox_lmbound = sandbox->linear_memory_size;
	// TODO: module table or sandbox table?
	module_indirect_table = sandbox->module->indirect_table;
}

/**
 * Check that the current_sandbox struct matches the rest of the thread local state about the executing sandbox
 * This includes lmbase, lmbound, and module_indirect_table
 */
static inline void
check_current_sandbox(void)
{
	struct sandbox *sandbox = get_current_sandbox();
	assert(sandbox && sandbox->linear_memory_start == sandbox_lmbase && sandbox->linear_memory_size == sandbox_lmbound);
	assert(sandbox->module->indirect_table == module_indirect_table);
}

/**
 * Given a sandbox, returns the module that sandbox is executing
 * @param sandbox the sandbox whose module we want
 * @return the module of the provided sandbox
 */
static inline struct module *
get_sandbox_module(struct sandbox *sandbox)
{
	if (!sandbox) return NULL;
	return sandbox->module;
}

extern void add_sandbox_to_completion_queue(struct sandbox *sandbox);

/**
 * @brief Switches to the next sandbox, placing the current sandbox of the completion queue if in RETURNED state
 * @param next The Sandbox Context to switch to or NULL
 * @return void
 */
static inline void
switch_to_sandbox(struct sandbox *next_sandbox)
{
	arch_context_t *next_register_context = next_sandbox == NULL ? NULL : &next_sandbox->ctxt;
	softint_disable();
	struct sandbox *current_sandbox          = get_current_sandbox();
	arch_context_t *current_register_context = current_sandbox == NULL ? NULL : &current_sandbox->ctxt;
	set_current_sandbox(next_sandbox);
	// If the current sandbox we're switching from is in a RETURNED state, add to completion queue
	if (current_sandbox && current_sandbox->state == RETURNED) add_sandbox_to_completion_queue(current_sandbox);
	next_context = next_register_context;
	arch_context_switch(current_register_context, next_register_context);
	softint_enable();
}

/**
 * Getter for the arguments of the current sandbox
 * @return the arguments of the current sandbox
 */
static inline char *
get_current_sandbox_arguments(void)
{
	struct sandbox *sandbox = get_current_sandbox();
	return (char *)sandbox->arguments;
}

void *          sandbox_worker_main(void *data);
struct sandbox *get_next_sandbox_from_local_run_queue(int interrupt);
void            block_current_sandbox(void);
void            wakeup_sandbox(sandbox_t *sb);
// called in sandbox_main() before and after fn() execution
// for http request/response processing using uvio
void sandbox_block_http(void);
void sandbox_response(void);

// should be the entry-point for each sandbox so it can do per-sandbox mem/etc init.
// should have been called with stack allocated and get_current_sandbox() set!
void                         sandbox_main(void);
void                         exit_current_sandbox(void);
extern struct deque_sandbox *global_deque;
extern pthread_mutex_t       global_deque_mutex;


/**
 * Pushes a sandbox request to the global deque
 * @param sandbox_request 
 **/
static inline int
add_sandbox_request_to_global_dequeue(sandbox_request_t *sandbox_request)
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

/**
 * Pops a sandbox request from the global deque
 * @param sandbox_request the pointer which we want to set to the sandbox request
 **/
static inline int
pop_sandbox_request_from_global_dequeue(sandbox_request_t **sandbox_request)
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
 * TODO: What does this do?
 * @returns A Sandbox Request or NULL
 **/
static inline sandbox_request_t *
steal_sandbox_request_from_global_dequeue(void)
{
	sandbox_request_t *sandbox_request = NULL;

#if NCORES == 1
	pop_sandbox_request_from_global_dequeue(&sandbox_request);
#else
	// TODO: check! is there a sandboxing thread on same core as udp-server thread?
	int r = deque_steal_sandbox(global_deque, &sandbox_request);
	if (r) sandbox_request = NULL;
#endif

	return sandbox_request;
}

/**
 * Initializes and returns an IO handle on the current sandbox ready for use
 * @return index of handle we preopened or -1 if all handles are exhausted
 **/
static inline int
initialize_io_handle_in_current_sandbox(void)
{
	struct sandbox *sandbox = get_current_sandbox();
	int             handle_index;
	for (handle_index = 0; handle_index < SBOX_MAX_OPEN; handle_index++) {
		if (sandbox->handles[handle_index].file_descriptor < 0) break;
	}
	if (handle_index == SBOX_MAX_OPEN) return -1;
	sandbox->handles[handle_index].file_descriptor = SBOX_PREOPEN_MAGIC;
	memset(&sandbox->handles[handle_index].libuv_handle, 0, sizeof(union uv_any_handle));
	return handle_index;
}

/**
 * Initializes and returns an IO handle on the current sandbox ready for use
 * @param file_descriptor what we'll set on the IO handle after initialization
 * @return index of handle we preopened or -1 if all handles are exhausted
 **/
static inline int
initialize_io_handle_and_set_file_descriptor_in_current_sandbox(int file_descriptor)
{
	struct sandbox *sandbox = get_current_sandbox();
	if (file_descriptor < 0) return file_descriptor;
	int handle_index                  = initialize_io_handle_in_current_sandbox();
	if (handle_index != -1) sandbox->handles[handle_index].file_descriptor = file_descriptor; // well, per sandbox.. so synchronization necessary!
	return handle_index;
}

/**
 * Sets the file descriptor of the sandbox's ith io_handle
 * Returns error condition if the file_descriptor to set does not contain sandbox preopen magin
 * @param handle_index index of the sandbox handles we want to set
 * @param file_descriptor the file descripter we want to set it to
 * @returns the index that was set or -1 in case of error
 **/
static inline int
set_current_sandbox_file_descriptor(int handle_index, int file_descriptor)
{
	struct sandbox *sandbox = get_current_sandbox();
	if (handle_index >= SBOX_MAX_OPEN || handle_index < 0) return -1;
	if (file_descriptor < 0 || sandbox->handles[handle_index].file_descriptor != SBOX_PREOPEN_MAGIC) return -1;
	sandbox->handles[handle_index].file_descriptor = file_descriptor;
	return handle_index;
}

/**
 * Get the file descriptor of the sandbox's ith io_handle
 * @param handle_index index into the sandbox's handles table
 * @returns file descriptor
 **/
static inline int
get_current_sandbox_file_descriptor(int handle_index)
{
	struct sandbox *sandbox = get_current_sandbox();
	if (handle_index >= SBOX_MAX_OPEN || handle_index < 0) return -1;
	return sandbox->handles[handle_index].file_descriptor;
}

/**
 * Close the sandbox's ith io_handle
 * @param handle_index index of the handle to close
 **/
static inline void
close_current_sandbox_file_descriptor(int handle_index)
{
	struct sandbox *sandbox = get_current_sandbox();
	if (handle_index >= SBOX_MAX_OPEN || handle_index < 0) return;
	// TODO: Do we actually need to call some sort of close function here?
	sandbox->handles[handle_index].file_descriptor = -1;
}

/**
 * Get the Libuv handle located at idx of the sandbox ith io_handle
 * @param handle_index index of the handle containing libuv_handle???
 * @returns any libuv handle
 **/
static inline union uv_any_handle *
get_current_sandbox_libuv_handle(int handle_index)
{
	struct sandbox *sandbox = get_current_sandbox();
	if (handle_index >= SBOX_MAX_OPEN || handle_index < 0) return NULL;
	return &sandbox->handles[handle_index].libuv_handle;
}

#endif /* SFRT_SANDBOX_H */
