#include "current_sandbox.h"

/* current sandbox that is active.. */
static __thread struct sandbox *worker_thread_current_sandbox = NULL;

__thread struct sandbox_context_cache local_sandbox_context_cache = {
	.linear_memory_start   = NULL,
	.linear_memory_size    = 0,
	.module_indirect_table = NULL,
};

/**
 * Getter for the current sandbox executing on this thread
 * @returns the current sandbox executing on this thread
 */
struct sandbox *
current_sandbox_get(void)
{
	return worker_thread_current_sandbox;
}

/**
 * Setter for the current sandbox executing on this thread
 * @param sandbox the sandbox we are setting this thread to run
 */
void
current_sandbox_set(struct sandbox *sandbox)
{
	/* Unpack hierarchy to avoid pointer chasing */
	if (sandbox == NULL) {
		local_sandbox_context_cache = (struct sandbox_context_cache){
			.linear_memory_start   = NULL,
			.linear_memory_size    = 0,
			.module_indirect_table = NULL,
		};
		worker_thread_current_sandbox = NULL;
	} else {
		local_sandbox_context_cache = (struct sandbox_context_cache){
			.linear_memory_start   = sandbox->linear_memory_start,
			.linear_memory_size    = sandbox->linear_memory_size,
			.module_indirect_table = sandbox->module->indirect_table,
		};
		worker_thread_current_sandbox = sandbox;
	}
}

/**
 * Initializes and returns an IO handle on the current sandbox ready for use
 * @return index of handle we preopened or -1 if all io_handles are exhausted
 */
int
current_sandbox_initialize_io_handle(void)
{
	return sandbox_initialize_io_handle(current_sandbox_get());
}

size_t sandbox_parse_http_request(struct sandbox *sandbox, size_t l);

/**
 * Sets the file descriptor of the sandbox's ith io_handle
 * Returns error condition if the file_descriptor to set does not contain sandbox preopen magin
 * @param io_handle_index index of the sandbox io_handle we want to set
 * @param file_descriptor the file descripter we want to set it to
 * @returns the index that was set or -1 in case of error
 */
int
current_sandbox_set_file_descriptor(int io_handle_index, int file_descriptor)
{
	return sandbox_set_file_descriptor(current_sandbox_get(), io_handle_index, file_descriptor);
}

/**
 * Get the file descriptor of the sandbox's ith io_handle
 * @param io_handle_index index into the sandbox's io_handles table
 * @returns file descriptor
 */
int
current_sandbox_get_file_descriptor(int io_handle_index)
{
	struct sandbox *sandbox = current_sandbox_get();
	return sandbox_get_file_descriptor(sandbox, io_handle_index);
}

/**
 * Close the sandbox's ith io_handle
 * @param io_handle_index index of the handle to close
 */
void
current_sandbox_close_file_descriptor(int io_handle_index)
{
	struct sandbox *sandbox = current_sandbox_get();
	sandbox_close_file_descriptor(sandbox, io_handle_index);
}
