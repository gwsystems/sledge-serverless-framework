#ifndef SFRT_SANDBOX_H
#define SFRT_SANDBOX_H

#include "ps_list.h"
#include "module.h"
#include "arch/context.h"
#include "softint.h"
#include <ucontext.h>
#include <uv.h>
#include "deque.h"
#include <http/http_request.h>
#include <http/http_response.h>

struct sandbox__io_handle {
	int                 file_descriptor;
	union uv_any_handle libuv_handle;
};

typedef enum
{
	RUNNABLE,
	BLOCKED,
	RETURNED
} sandbox__state_t;

/*
 * This is the slowpath switch to a preempted sandbox!
 * SIGUSR1 on the current thread and restore mcontext there!
 */
extern void __attribute__((noreturn)) sandbox_switch_preempt(void);

// TODO: linear_memory_max_size is not really used

struct sandbox {
	sandbox__state_t state;

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

	struct sandbox__io_handle     handles[SBOX_MAX_OPEN];
	struct sockaddr      client_address; // client requesting connection!
	int                  client_socket_descriptor;
	uv_tcp_t             client_libuv_stream;
	uv_shutdown_t        client_libuv_shutdown_request;

	http_parser          http_parser;
	struct http_request  http_request;
	struct http_response http_response;

	char *  read_buffer;
	ssize_t read_length, read_size;

	// Used by the ps_list macro
	struct ps_list list;

	ssize_t request_response_data_length;      // <= max(module->max_request_or_response_size)
	char    request_response_data[1]; // of rr_data_sz, following sandbox mem..
} PAGE_ALIGNED;

extern __thread struct sandbox *current_sandbox;
// next_sandbox only used in SIGUSR1
extern __thread arch_context_t *next_context;

typedef struct sandbox sandbox_t;
extern void add_sandbox_to_completion_queue(struct sandbox *sandbox);

/***************************
 * Sandbox                 *
 **************************/

// a runtime resource, malloc on this!
struct sandbox *sandbox__allocate(struct module *module, char *arguments, int socket_descriptor, const struct sockaddr *socket_address, u64 start_time);
// should free stack and heap resources.. also any I/O handles.
void sandbox__free(struct sandbox *sandbox);


/**
 * Given a sandbox, returns the module that sandbox is executing
 * @param sandbox the sandbox whose module we want
 * @return the module of the provided sandbox
 */
static inline struct module *
sandbox__get_module(struct sandbox *sandbox)
{
	if (!sandbox) return NULL;
	return sandbox->module;
}

/**
 * Getter for the arguments of the sandbox
 * @param sandbox
 * @return the arguments of the sandbox
 */
static inline char *
sandbox__get_arguments(struct sandbox *sandbox)
{
	if (!sandbox) return NULL;
	return (char *)sandbox->arguments;
}

/**
 * Initializes and returns an IO handle on the current sandbox ready for use
 * @param sandbox
 * @return index of handle we preopened or -1 on error (sandbox is null or all handles are exhausted)
 **/
static inline int
sandbox__initialize_io_handle(struct sandbox *sandbox)
{
	if (!sandbox) return -1;
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
 * Initializes and returns an IO handle of a sandbox ready for use
 * @param sandbox
 * @param file_descriptor what we'll set on the IO handle after initialization
 * @return index of handle we preopened or -1 if all handles are exhausted
 **/
static inline int
sandbox__initialize_io_handle_and_set_file_descriptor(struct sandbox *sandbox, int file_descriptor)
{
	if (!sandbox) return -1;
	if (file_descriptor < 0) return file_descriptor;
	int handle_index                  = sandbox__initialize_io_handle(sandbox);
	if (handle_index != -1) sandbox->handles[handle_index].file_descriptor = file_descriptor; // well, per sandbox.. so synchronization necessary!
	return handle_index;
}

/**
 * Sets the file descriptor of the sandbox's ith io_handle
 * Returns error condition if the file_descriptor to set does not contain sandbox preopen magic
 * @param sandbox
 * @param handle_index index of the sandbox handles we want to set
 * @param file_descriptor the file descripter we want to set it to
 * @returns the index that was set or -1 in case of error
 **/
static inline int
sandbox__set_file_descriptor(struct sandbox *sandbox, int handle_index, int file_descriptor)
{
	if (!sandbox) return -1;
	if (handle_index >= SBOX_MAX_OPEN || handle_index < 0) return -1;
	if (file_descriptor < 0 || sandbox->handles[handle_index].file_descriptor != SBOX_PREOPEN_MAGIC) return -1;
	sandbox->handles[handle_index].file_descriptor = file_descriptor;
	return handle_index;
}

/**
 * Get the file descriptor of the sandbox's ith io_handle
 * @param sandbox
 * @param handle_index index into the sandbox's handles table
 * @returns file descriptor or -1 in case of error
 **/
static inline int
sandbox__get_file_descriptor(struct sandbox *sandbox, int handle_index)
{
	if (!sandbox) return -1;
	if (handle_index >= SBOX_MAX_OPEN || handle_index < 0) return -1;
	return sandbox->handles[handle_index].file_descriptor;
}

/**
 * Close the sandbox's ith io_handle
 * @param sandbox
 * @param handle_index index of the handle to close
 **/
static inline void
sandbox__close_file_descriptor(struct sandbox *sandbox, int handle_index)
{
	if (handle_index >= SBOX_MAX_OPEN || handle_index < 0) return;
	// TODO: Do we actually need to call some sort of close function here?
	sandbox->handles[handle_index].file_descriptor = -1;
}

/**
 * Get the Libuv handle located at idx of the sandbox ith io_handle
 * @param sandbox
 * @param handle_index index of the handle containing libuv_handle???
 * @returns any libuv handle or a NULL pointer in case of error
 **/
static inline union uv_any_handle *
sandbox__get_libuv_handle(struct sandbox *sandbox, int handle_index)
{
	if (!sandbox) return NULL;
	if (handle_index >= SBOX_MAX_OPEN || handle_index < 0) return NULL;
	return &sandbox->handles[handle_index].libuv_handle;
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
// should have been called with stack allocated and current_sandbox__get() set!
void                         sandbox_main(void);
void                         current_sandbox__exit(void);
int sandbox__parse_http_request(struct sandbox *sandbox, size_t length);

#endif /* SFRT_SANDBOX_H */
