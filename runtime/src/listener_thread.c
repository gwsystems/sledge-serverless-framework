#include <stdint.h>
#include <unistd.h>

#include "arch/getcycles.h"
#include "client_socket.h"
#include "global_request_scheduler.h"
#include "generic_thread.h"
#include "listener_thread.h"
#include "runtime.h"
#include "sandbox_functions.h"

/*
 * Descriptor of the epoll instance used to monitor the socket descriptors of registered
 * serverless modules. The listener cores listens for incoming client requests through this.
 */
int listener_thread_epoll_file_descriptor;

pthread_t listener_thread_id;

/**
 * Initializes the listener thread, pinned to core 0, and starts to listen for requests
 */
void
listener_thread_initialize(void)
{
	printf("Starting listener thread\n");
	cpu_set_t cs;

	CPU_ZERO(&cs);
	CPU_SET(LISTENER_THREAD_CORE_ID, &cs);

	/* Setup epoll */
	listener_thread_epoll_file_descriptor = epoll_create1(0);
	assert(listener_thread_epoll_file_descriptor >= 0);

	int ret = pthread_create(&listener_thread_id, NULL, listener_thread_main, NULL);
	assert(ret == 0);
	ret = pthread_setaffinity_np(listener_thread_id, sizeof(cpu_set_t), &cs);
	assert(ret == 0);
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cs);
	assert(ret == 0);

	printf("\tListener core thread: %lx\n", listener_thread_id);
}

/**
 * @brief Registers a serverless module on the listener thread's epoll descriptor
 **/
int
listener_thread_register_module(struct module *mod)
{
	assert(mod != NULL);
	if (unlikely(listener_thread_epoll_file_descriptor == 0)) {
		panic("Attempting to register a module before listener thread initialization");
	}

	int                rc = 0;
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)mod;
	accept_evt.events   = EPOLLIN;
	rc = epoll_ctl(listener_thread_epoll_file_descriptor, EPOLL_CTL_ADD, mod->socket_descriptor, &accept_evt);

	return rc;
}

/**
 * @brief Execution Loop of the listener core, io_handles HTTP requests, allocates sandbox request objects, and
 * pushes the sandbox object to the global dequeue
 * @param dummy data pointer provided by pthreads API. Unused in this function
 * @return NULL
 *
 * Used Globals:
 * listener_thread_epoll_file_descriptor - the epoll file descriptor
 *
 */
noreturn void *
listener_thread_main(void *dummy)
{
	struct epoll_event epoll_events[RUNTIME_MAX_EPOLL_EVENTS];

	generic_thread_initialize();

	/* Set my priority */
	// runtime_set_pthread_prio(pthread_self(), 2);
	pthread_setschedprio(pthread_self(), -20);

	while (true) {
		/*
		 * Block indefinitely on the epoll file descriptor, waiting on up to a max number of events
		 * TODO: Is RUNTIME_MAX_EPOLL_EVENTS actually limited to the max number of modules?
		 */
		int descriptor_count = epoll_wait(listener_thread_epoll_file_descriptor, epoll_events,
		                                  RUNTIME_MAX_EPOLL_EVENTS, -1);
		if (descriptor_count < 0) {
			if (errno == EINTR) continue;

			panic("epoll_wait: %s", strerror(errno));
		}
		/* Assumption: Because epoll_wait is set to not timeout, we should always have descriptors here
		 */
		assert(descriptor_count > 0);

		uint64_t request_arrival_timestamp = __getcycles();
		for (int i = 0; i < descriptor_count; i++) {
			/* Check Event to determine if epoll returned an error */
			if ((epoll_events[i].events & EPOLLERR) == EPOLLERR) {
				int       error  = 0;
				socklen_t errlen = sizeof(error);
				if (getsockopt(epoll_events[i].data.fd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen)
				    == 0) {
					panic("epoll_wait: %s\n", strerror(error));
				}
				panic("epoll_wait");
			};

			/* Assumption: We have only registered EPOLLIN events, so we should see no others here
			 */
			assert((epoll_events[i].events & EPOLLIN) == EPOLLIN);

			/* Unpack module from epoll event */
			struct module *module = (struct module *)epoll_events[i].data.ptr;
			assert(module);

			/*
			 * I don't think we're responsible to cleanup epoll events, but clearing to trigger
			 * the assertion just in case.
			 */
			epoll_events[i].data.ptr = NULL;

			/* Accept Client Request as a nonblocking socket, saving address information */
			struct sockaddr_in client_address;
			socklen_t          address_length = sizeof(client_address);

			/*
			 * Accept as many requests as possible, terminating when we would have blocked
			 * This inner loop is used in case there are more datagrams than epoll events for some
			 * reason
			 */
			while (true) {
				int client_socket = accept4(module->socket_descriptor,
				                            (struct sockaddr *)&client_address, &address_length,
				                            SOCK_NONBLOCK);
				if (unlikely(client_socket < 0)) {
					if (errno == EWOULDBLOCK || errno == EAGAIN) break;

					panic("accept4: %s", strerror(errno));
				}

				/* We should never have accepted on fd 0, 1, or 2 */
				assert(client_socket != STDIN_FILENO);
				assert(client_socket != STDOUT_FILENO);
				assert(client_socket != STDERR_FILENO);

				/*
				 * According to accept(2), it is possible that the the sockaddr structure
				 * client_address may be too small, resulting in data being truncated to fit.
				 * The accept call mutates the size value to indicate that this is the case.
				 */
				if (address_length > sizeof(client_address)) {
					debuglog("Client address %s truncated because buffer was too small\n",
					         module->name);
				}

				http_total_increment_request();

				/* Allocate HTTP Session */
				struct http_session *session =
				  http_session_alloc(module->max_request_size, module->max_response_size, client_socket,
				                     (const struct sockaddr *)&client_address);

				/* TODO: Read HTTP request */

				/*
				 * Perform admissions control.
				 * If 0, workload was rejected, so close with 429 "Too Many Requests" and continue
				 * TODO: Consider providing a Retry-After header
				 */
				uint64_t work_admitted = admissions_control_decide(module->admissions_info.estimate);
				if (work_admitted == 0) {
					client_socket_send_oneshot(client_socket, http_header_build(429),
					                           http_header_len(429));
					if (unlikely(close(client_socket) < 0))
						debuglog("Error closing client socket - %s", strerror(errno));

					continue;
				}

				/* Allocate a Sandbox */
				struct sandbox *sandbox = sandbox_alloc(module, session, request_arrival_timestamp,
				                                        work_admitted);
				if (unlikely(sandbox == NULL)) {
					http_session_send_err_oneshot(sandbox->http, 503);
					http_session_close(sandbox->http);
				}

				/* If the global request scheduler is full, return a 429 to the client */
				sandbox = global_request_scheduler_add(sandbox);
				if (unlikely(sandbox == NULL)) {
					http_session_send_err_oneshot(sandbox->http, 429);
					http_session_close(sandbox->http);
				}

			} /* while true */
		}         /* for loop */
		generic_thread_dump_lock_overhead();
	} /* while true */

	panic("Listener thread unexpectedly broke loop\n");
}
