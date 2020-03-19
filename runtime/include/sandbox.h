#ifndef SFRT_SANDBOX_H
#define SFRT_SANDBOX_H

#include <ucontext.h>
#include <uv.h>

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
	RUNNABLE,
	BLOCKED,
	RETURNED
} sandbox_state_t;

// TODO: linear_memory_max_size is not really used

struct sandbox {
	sandbox_state_t state;

	u32 sandbox_size; // The struct plus enough buffer to hold the request or response (sized off largest)

	void *linear_memory_start; // after sandbox struct
	u32   linear_memory_size;  // from after sandbox struct
	u32   linear_memory_max_size;

	void *stack_start; // guess we need a mechanism for stack allocation.
	u32   stack_size;  // and to set the size of it.

	arch_context_t ctxt; // register context for context switch.

	u64 total_time;
	u64 start_time;

	struct module *module; // the module this is an instance of

	i32   arguments_offset; // actual placement of arguments in the sandbox.
	void *arguments;        // arguments from request, must be of module->argument_count size.
	i32   return_value;

	struct sandbox_io_handle io_handles[SANDBOX__MAX_IO_HANDLE_COUNT];
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


extern __thread struct sandbox *worker_thread__current_sandbox;
extern __thread arch_context_t *worker_thread__next_context;

extern void            worker_thread__block_current_sandbox(void);
extern void            worker_thread__exit_current_sandbox(void);
extern struct sandbox *worker_thread__get_next_sandbox(int interrupt);
extern void            worker_thread__process_io(void);
extern void            worker_thread__push_sandbox_to_completion_queue(struct sandbox *sandbox);
extern void __attribute__((noreturn)) worker_thread__sandbox_switch_preempt(void);
extern void worker_thread__wakeup_sandbox(sandbox_t *sandbox);

/***************************
 * Public API              *
 **************************/

struct sandbox *sandbox_allocate(struct module *module, char *arguments, int socket_descriptor,
                                 const struct sockaddr *socket_address, u64 start_time);
void            sandbox_free(struct sandbox *sandbox);
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
	for (io_handle_index = 0; io_handle_index < SANDBOX__MAX_IO_HANDLE_COUNT; io_handle_index++) {
		if (sandbox->io_handles[io_handle_index].file_descriptor < 0) break;
	}
	if (io_handle_index == SANDBOX__MAX_IO_HANDLE_COUNT) return -1;
	sandbox->io_handles[io_handle_index].file_descriptor = SANDBOX__FILE_DESCRIPTOR_PREOPEN_MAGIC;
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
	if (io_handle_index >= SANDBOX__MAX_IO_HANDLE_COUNT || io_handle_index < 0) return -1;
	if (file_descriptor < 0
	    || sandbox->io_handles[io_handle_index].file_descriptor != SANDBOX__FILE_DESCRIPTOR_PREOPEN_MAGIC)
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
	if (io_handle_index >= SANDBOX__MAX_IO_HANDLE_COUNT || io_handle_index < 0) return -1;
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
	if (io_handle_index >= SANDBOX__MAX_IO_HANDLE_COUNT || io_handle_index < 0) return;
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
	if (io_handle_index >= SANDBOX__MAX_IO_HANDLE_COUNT || io_handle_index < 0) return NULL;
	return &sandbox->io_handles[io_handle_index].libuv_handle;
}

#endif /* SFRT_SANDBOX_H */
