
#ifndef SFRT_LIBUV_CALLBACKS_H
#define SFRT_LIBUV_CALLBACKS_H

#include <assert.h>
#include <runtime.h>
#include <sandbox.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>
#include <uv.h>
#include <http_request.h>

/**
 *  TODO: is there some weird edge case where a UNICODE character might be split between reads? Do we care?
 * Called after libuv has read a chunk of data
 * Parses data read by the libuv stream chunk-by-chunk until the message is complete
 * Then stops the stream and wakes up the sandbox
 * @param stream
 * @param number_read bytes read
 * @param buffer unused
 **/
static inline void
libuv_callbacks_on_read_parse_http_request(uv_stream_t *stream, ssize_t number_read, const uv_buf_t *buffer)
{
	struct sandbox *sandbox = stream->data;

	// Parse the chunks libuv has read on our behalf until we've parse to message end
	if (number_read > 0) {
		if (sandbox_parse_http_request(sandbox, number_read) != 0) return;
		sandbox->request_response_data_length += number_read;
		struct http_request *rh = &sandbox->http_request;
		if (!rh->message_end) return;
	}

	// When the entire message has been read, stop the stream and wakeup the sandbox
	uv_read_stop(stream);
	worker_thread_wakeup_sandbox(sandbox);
}

/**
 * On libuv close, executes this callback to wake the blocked sandbox back up
 * @param stream
 **/
static inline void
libuv_callbacks_on_close_wakeup_sakebox(uv_handle_t *stream)
{
	struct sandbox *sandbox = stream->data;
	worker_thread_wakeup_sandbox(sandbox);
}

/**
 * On libuv shutdown, executes this callback to wake the blocked sandbox back up
 * @param req shutdown request
 * @param status unused in callback
 **/
static inline void
libuv_callbacks_on_shutdown_wakeup_sakebox(uv_shutdown_t *req, int status)
{
	struct sandbox *sandbox = req->data;
	worker_thread_wakeup_sandbox(sandbox);
}

/**
 * On libuv write, executes this callback to wake the blocked sandbox back up
 * In case of error, shutdown the sandbox
 * @param write shutdown request
 * @param status status code
 **/
static inline void
libuv_callbacks_on_write_wakeup_sandbox(uv_write_t *write, int status)
{
	struct sandbox *sandbox = write->data;
	if (status < 0) {
		sandbox->client_libuv_shutdown_request.data = sandbox;
		uv_shutdown(&sandbox->client_libuv_shutdown_request, (uv_stream_t *)&sandbox->client_libuv_stream,
		            libuv_callbacks_on_shutdown_wakeup_sakebox);
		return;
	}
	worker_thread_wakeup_sandbox(sandbox);
}

static inline void
libuv_callbacks_on_allocate_setup_request_response_data(uv_handle_t *h, size_t suggested, uv_buf_t *buf)
{
	struct sandbox *sandbox = h->data;
	size_t          l = (sandbox->module->max_request_or_response_size - sandbox->request_response_data_length);
	buf->base         = (sandbox->request_response_data + sandbox->request_response_data_length);
	buf->len          = l > suggested ? suggested : l;
}

#endif /* SFRT_SANDBOX_H */
