#ifndef SFRT_CURRENT_SANDBOX_H
#define SFRT_CURRENT_SANDBOX_H

#include <sandbox.h>
#include <types.h>

/**
 * Getter for the current sandbox executing on this thread
 * @returns the current sandbox executing on this thread
 **/
static inline struct sandbox *
current_sandbox__get(void)
{
	return current_sandbox;
}

/**
 * Setter for the current sandbox executing on this thread
 * @param sandbox the sandbox we are setting this thread to run
 **/
static inline void
current_sandbox__set(struct sandbox *sandbox)
{
	// FIXME: critical-section.
	current_sandbox = sandbox;
	if (sandbox == NULL) return;

	// Thread Local State about the Current Sandbox
	sandbox_lmbase  = sandbox->linear_memory_start;
	sandbox_lmbound = sandbox->linear_memory_size;
	module_indirect_table = sandbox->module->indirect_table;
}

/**
 * Getter for the arguments of the current sandbox
 * @return the arguments of the current sandbox
 */
static inline char *
current_sandbox__get_arguments(void)
{
	struct sandbox *sandbox = current_sandbox__get();
	return sandbox__get_arguments(sandbox);
}

/**
 * Initializes and returns an IO handle on the current sandbox ready for use
 * @return index of handle we preopened or -1 if all handles are exhausted
 **/
static inline int
current_sandbox__initialize_io_handle(void)
{
	struct sandbox *sandbox = current_sandbox__get();
	return sandbox__initialize_io_handle(sandbox);
}

/**
 * Initializes and returns an IO handle on the current sandbox ready for use
 * @param file_descriptor what we'll set on the IO handle after initialization
 * @return index of handle we preopened or -1 if all handles are exhausted
 **/
static inline int
current_sandbox__initialize_io_handle_and_set_file_descriptor(int file_descriptor)
{
	struct sandbox *sandbox = current_sandbox__get();
	return sandbox__initialize_io_handle_and_set_file_descriptor(sandbox, file_descriptor);
}

extern http_parser_settings global__http_parser_settings;
int sandbox__parse_http_request(struct sandbox *sandbox, size_t l);

/**
 * Parse the current sandbox's request_response_data up to length
 * @param length
 * @returns 0
 **/
static inline int
current_sandbox__parse_http_request(size_t length)
{
	return sandbox__parse_http_request(current_sandbox__get(), length);
}

/**
 * Sets the file descriptor of the sandbox's ith io_handle
 * Returns error condition if the file_descriptor to set does not contain sandbox preopen magin
 * @param handle_index index of the sandbox handles we want to set
 * @param file_descriptor the file descripter we want to set it to
 * @returns the index that was set or -1 in case of error
 **/
static inline int
current_sandbox__set_file_descriptor(int handle_index, int file_descriptor)
{
	struct sandbox *sandbox = current_sandbox__get();
	return sandbox__set_file_descriptor(sandbox, handle_index, file_descriptor);
}

/**
 * Get the file descriptor of the sandbox's ith io_handle
 * @param handle_index index into the sandbox's handles table
 * @returns file descriptor
 **/
static inline int
current_sandbox__get_file_descriptor(int handle_index)
{
	struct sandbox *sandbox = current_sandbox__get();
	return sandbox__get_file_descriptor(sandbox, handle_index);
}

/**
 * Close the sandbox's ith io_handle
 * @param handle_index index of the handle to close
 **/
static inline void
current_sandbox__close_file_descriptor(int handle_index)
{
	struct sandbox *sandbox = current_sandbox__get();
	sandbox__close_file_descriptor(sandbox, handle_index);
}

/**
 * Get the Libuv handle located at idx of the sandbox ith io_handle
 * @param handle_index index of the handle containing libuv_handle???
 * @returns any libuv handle
 **/
static inline union uv_any_handle *
current_sandbox__get_libuv_handle(int handle_index)
{
	struct sandbox *sandbox = current_sandbox__get();
	return sandbox__get_libuv_handle(sandbox, handle_index);
}

/**
 * Gets the HTTP Request body from the current sandbox
 * @param body pointer that we'll assign to the http_request body
 * @returns the length of the http_request's body
 **/
static inline int
current_sandbox__get_http_request_body(char **body)
{
	return http_request__get_body(&current_sandbox__get()->http_request, body);
}


/**
 * Set an HTTP Response Header on the current sandbox
 * @param header string of the header that we want to set
 * @param length the length of the header string
 * @returns 0 (abends program in case of error)
 **/
static inline int
current_sandbox__set_http_response_header(char *header, int length)
{
	return http_response__set_header(&current_sandbox__get()->http_response, header, length);
}

/**
 * Set an HTTP Response Body on the current sandbox
 * @param body string of the body that we want to set
 * @param length the length of the body string
 * @returns 0 (abends program in case of error)
 **/
static inline int
current_sandbox__set_http_response_body(char *body, int length)
{
	return http_response__set_body(&current_sandbox__get()->http_response, body, length);
}

/**
 * Set an HTTP Response Status on the current sandbox
 * @param status string of the status we want to set
 * @param length the length of the status
 * @returns 0 (abends program in case of error)
 **/
static inline int
current_sandbox__set_http_response_status(char *status, int length)
{
	return http_response__set_status(&current_sandbox__get()->http_response, status, length);
}

/**
 * Encode the current sandbox's HTTP Response as an array of buffers
 * @returns the number of buffers used to store the HTTP Response
 **/
static inline int
current_sandbox__vectorize_http_response(void)
{
	return http_response__encode_as_vector(&current_sandbox__get()->http_response);
}


#endif /* SFRT_CURRENT_SANDBOX_H */
