#ifndef SFRT_SANDBOX_H
#define SFRT_SANDBOX_H

#include <ucontext.h>
#include <uv.h>
#include <stdbool.h>
#include "sandbox_request.h"

#include "arch/context.h"
#include "deque.h"
#include <http_request.h>
#include <http_response.h>
#include "module.h"
#include "ps_list.h"
#include "software_interrupt.h"

/**************************
 * Structs and Types      *
 **************************/

struct sandbox_io_handle {
	int                 file_descriptor;
	union uv_any_handle libuv_handle;
};

typedef enum
{
	SANDBOX_UNINITIALIZED, // Assuming that mmap zeros out data. Uninitialized?
	SANDBOX_SET_AS_INITIALIZED,
	SANDBOX_INITIALIZED,
	SANDBOX_SET_AS_RUNNABLE,
	SANDBOX_RUNNABLE,
	SANDBOX_SET_AS_RUNNING,
	SANDBOX_RUNNING,
	SANDBOX_SET_AS_PREEMPTED,
	SANDBOX_PREEMPTED,
	SANDBOX_SET_AS_BLOCKED,
	SANDBOX_BLOCKED,
	SANDBOX_SET_AS_RETURNED,
	SANDBOX_RETURNED,
	SANDBOX_SET_AS_COMPLETE,
	SANDBOX_COMPLETE,
	SANDBOX_SET_AS_ERROR,
	SANDBOX_ERROR,
	SANDBOX_STATE_COUNT
} sandbox_state_t;

struct sandbox {
	sandbox_state_t state;

	uint32_t sandbox_size; // The struct plus enough buffer to hold the request or response (sized off largest)

	void *   linear_memory_start; // after sandbox struct
	uint32_t linear_memory_size;  // from after sandbox struct
	uint64_t linear_memory_max_size;

	void *   stack_start; // guess we need a mechanism for stack allocation.
	uint32_t stack_size;  // and to set the size of it.

	struct arch_context ctxt; // register context for context switch.

	uint64_t request_timestamp;           // Timestamp when request is received
	uint64_t allocation_timestamp;        // Timestamp when sandbox is allocated
	uint64_t response_timestamp;          // Timestamp when response is sent
	uint64_t completion_timestamp;        // Timestamp when sandbox runs to completion
	uint64_t last_state_change_timestamp; // Used for bookkeeping of actual execution time

	// Duration of time (in cycles) that the sandbox is in each state
	uint64_t initializing_duration;
	uint64_t runnable_duration;
	uint64_t preempted_duration;
	uint64_t running_duration;
	uint64_t blocked_duration;
	uint64_t returned_duration;

	uint64_t absolute_deadline;
	uint64_t total_time; // From Request to Response

	struct module *module; // the module this is an instance of

	int32_t arguments_offset; // actual placement of arguments in the sandbox.
	void *  arguments;        // arguments from request, must be of module->argument_count size.
	int32_t return_value;

	struct sandbox_io_handle io_handles[SANDBOX_MAX_IO_HANDLE_COUNT];
	struct sockaddr          client_address; // client requesting connection!
	int                      client_socket_descriptor;
	uv_tcp_t                 client_libuv_stream;
	uv_shutdown_t            client_libuv_shutdown_request;

	http_parser          http_parser;
	struct http_request  http_request;
	struct http_response http_response;

	char *  read_buffer;
	ssize_t read_length, read_size;

	// Used for the scheduling runqueue as an in-place linked list data structure.
	// The variable name "list" is used for ps_list's default name-based MACROS.
	struct ps_list list;

	ssize_t request_response_data_length; // <= max(module->max_request_or_response_size)
	char    request_response_data[1];     // of request_response_data_length, following sandbox mem..
} PAGE_ALIGNED;

typedef struct sandbox sandbox_t;

/***************************
 * Externs                 *
 **************************/


extern __thread volatile bool worker_thread_is_switching_context;

extern void worker_thread_block_current_sandbox(void);
extern void worker_thread_on_sandbox_exit(sandbox_t *sandbox);
extern void worker_thread_process_io(void);
extern void __attribute__((noreturn)) worker_thread_mcontext_restore(void);
extern void worker_thread_wakeup_sandbox(sandbox_t *sandbox);

/***************************
 * Public API              *
 **************************/

struct sandbox *sandbox_allocate(sandbox_request_t *sandbox_request);
void            sandbox_free(struct sandbox *sandbox);
void            sandbox_free_linear_memory(struct sandbox *sandbox);
char *          sandbox_state_stringify(sandbox_state_t sandbox_state);
void            sandbox_main(struct sandbox *sandbox);
int             sandbox_parse_http_request(struct sandbox *sandbox, size_t length);


/**
 * Given a sandbox, returns the module that sandbox is executing
 * @param sandbox the sandbox whose module we want
 * @return the module of the provided sandbox
 */
static inline struct module *
sandbox_get_module(struct sandbox *sandbox)
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
sandbox_get_arguments(struct sandbox *sandbox)
{
	if (!sandbox) return NULL;
	return (char *)sandbox->arguments;
}

/**
 * Initializes and returns an IO handle on the current sandbox ready for use
 * @param sandbox
 * @return index of handle we preopened or -1 on error (sandbox is null or all io_handles are exhausted)
 **/
static inline int
sandbox_initialize_io_handle(struct sandbox *sandbox)
{
	if (!sandbox) return -1;
	int io_handle_index;
	for (io_handle_index = 0; io_handle_index < SANDBOX_MAX_IO_HANDLE_COUNT; io_handle_index++) {
		if (sandbox->io_handles[io_handle_index].file_descriptor < 0) break;
	}
	if (io_handle_index == SANDBOX_MAX_IO_HANDLE_COUNT) return -1;
	sandbox->io_handles[io_handle_index].file_descriptor = SANDBOX_FILE_DESCRIPTOR_PREOPEN_MAGIC;
	memset(&sandbox->io_handles[io_handle_index].libuv_handle, 0, sizeof(union uv_any_handle));
	return io_handle_index;
}


/**
 * Initializes and returns an IO handle of a sandbox ready for use
 * @param sandbox
 * @param file_descriptor what we'll set on the IO handle after initialization
 * @return index of handle we preopened or -1 if all io_handles are exhausted
 **/
static inline int
sandbox_initialize_io_handle_and_set_file_descriptor(struct sandbox *sandbox, int file_descriptor)
{
	if (!sandbox) return -1;
	if (file_descriptor < 0) return file_descriptor;
	int io_handle_index = sandbox_initialize_io_handle(sandbox);
	if (io_handle_index != -1)
		sandbox->io_handles[io_handle_index].file_descriptor =
		  file_descriptor; // well, per sandbox.. so synchronization necessary!
	return io_handle_index;
}

/**
 * Sets the file descriptor of the sandbox's ith io_handle
 * Returns error condition if the file_descriptor to set does not contain sandbox preopen magic
 * @param sandbox
 * @param io_handle_index index of the sandbox io_handles we want to set
 * @param file_descriptor the file descripter we want to set it to
 * @returns the index that was set or -1 in case of error
 **/
static inline int
sandbox_set_file_descriptor(struct sandbox *sandbox, int io_handle_index, int file_descriptor)
{
	if (!sandbox) return -1;
	if (io_handle_index >= SANDBOX_MAX_IO_HANDLE_COUNT || io_handle_index < 0) return -1;
	if (file_descriptor < 0
	    || sandbox->io_handles[io_handle_index].file_descriptor != SANDBOX_FILE_DESCRIPTOR_PREOPEN_MAGIC)
		return -1;
	sandbox->io_handles[io_handle_index].file_descriptor = file_descriptor;
	return io_handle_index;
}

/**
 * Get the file descriptor of the sandbox's ith io_handle
 * @param sandbox
 * @param io_handle_index index into the sandbox's io_handles table
 * @returns file descriptor or -1 in case of error
 **/
static inline int
sandbox_get_file_descriptor(struct sandbox *sandbox, int io_handle_index)
{
	if (!sandbox) return -1;
	if (io_handle_index >= SANDBOX_MAX_IO_HANDLE_COUNT || io_handle_index < 0) return -1;
	return sandbox->io_handles[io_handle_index].file_descriptor;
}

/**
 * Close the sandbox's ith io_handle
 * @param sandbox
 * @param io_handle_index index of the handle to close
 **/
static inline void
sandbox_close_file_descriptor(struct sandbox *sandbox, int io_handle_index)
{
	if (io_handle_index >= SANDBOX_MAX_IO_HANDLE_COUNT || io_handle_index < 0) return;
	// TODO: Do we actually need to call some sort of close function here?
	sandbox->io_handles[io_handle_index].file_descriptor = -1;
}

/**
 * Get the Libuv handle located at idx of the sandbox ith io_handle
 * @param sandbox
 * @param io_handle_index index of the handle containing libuv_handle???
 * @returns any libuv handle or a NULL pointer in case of error
 **/
static inline union uv_any_handle *
sandbox_get_libuv_handle(struct sandbox *sandbox, int io_handle_index)
{
	if (!sandbox) return NULL;
	if (io_handle_index >= SANDBOX_MAX_IO_HANDLE_COUNT || io_handle_index < 0) return NULL;
	return &sandbox->io_handles[io_handle_index].libuv_handle;
}

void sandbox_set_as_initialized(sandbox_t *sandbox, sandbox_request_t *sandbox_request, uint64_t allocation_timestamp);
void sandbox_set_as_runnable(sandbox_t *sandbox);
void sandbox_set_as_running(sandbox_t *sandbox);
void sandbox_set_as_blocked(sandbox_t *sandbox);
void sandbox_set_as_preempted(sandbox_t *sandbox);
void sandbox_set_as_returned(sandbox_t *sandbox);
void sandbox_set_as_complete(sandbox_t *sandbox);
void sandbox_set_as_error(sandbox_t *sandbox);

#endif /* SFRT_SANDBOX_H */
