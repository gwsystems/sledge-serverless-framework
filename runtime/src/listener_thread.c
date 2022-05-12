#include <stdint.h>
#include <unistd.h>

#include "arch/getcycles.h"
#include "global_request_scheduler.h"
#include "generic_thread.h"
#include "listener_thread.h"
#include "module.h"
#include "runtime.h"
#include "sandbox_functions.h"
#include "tcp_session.h"
#include "tenant.h"
#include "tenant_functions.h"

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
 * @brief Registers a serverless tenant on the listener thread's epoll descriptor
 **/
void
listener_thread_register_http_session(struct http_session *http)
{
	assert(http != NULL);

	if (unlikely(listener_thread_epoll_file_descriptor == 0)) {
		panic("Attempting to register an http session before listener thread initialization");
	}

	int                rc = 0;
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)http;
	accept_evt.events   = EPOLLIN;

	epoll_ctl(listener_thread_epoll_file_descriptor, EPOLL_CTL_ADD, http->socket, &accept_evt);
}

/**
 * @brief Registers a serverless tenant on the listener thread's epoll descriptor
 **/
void
listener_thread_unregister_http_session(struct http_session *http)
{
	assert(http != NULL);

	if (unlikely(listener_thread_epoll_file_descriptor == 0)) {
		panic("Attempting to unregister an http session before listener thread initialization");
	}

	epoll_ctl(listener_thread_epoll_file_descriptor, EPOLL_CTL_DEL, http->socket, NULL);
}

/**
 * @brief Registers a serverless tenant on the listener thread's epoll descriptor
 **/
int
listener_thread_register_tenant(struct tenant *tenant)
{
	assert(tenant != NULL);
	if (unlikely(listener_thread_epoll_file_descriptor == 0)) {
		panic("Attempting to register a tenant before listener thread initialization");
	}

	int                rc = 0;
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)tenant;
	accept_evt.events   = EPOLLIN;
	rc = epoll_ctl(listener_thread_epoll_file_descriptor, EPOLL_CTL_ADD, tenant->tcp_server.socket_descriptor,
	               &accept_evt);

	return rc;
}

static void
panic_on_epoll_error(struct epoll_event *evt)
{
	/* Check Event to determine if epoll returned an error */
	if ((evt->events & EPOLLERR) == EPOLLERR) {
		int       error  = 0;
		socklen_t errlen = sizeof(error);
		if (getsockopt(evt->data.fd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen) == 0) {
			panic("epoll_wait: %s\n", strerror(error));
		}
		panic("epoll_wait");
	};
}

static void
handle_tcp_requests(struct epoll_event *evt)
{
	assert((evt->events & EPOLLIN) == EPOLLIN);

	/* Unpack tenant from epoll event */
	struct tenant *tenant = (struct tenant *)evt->data.ptr;
	assert(tenant);

	/* Accept Client Request as a nonblocking socket, saving address information */
	struct sockaddr_in client_address;
	socklen_t          address_length = sizeof(client_address);

	/* Accept as many requests as possible, returning when we would have blocked */
	while (true) {
		int client_socket = accept4(tenant->tcp_server.socket_descriptor, (struct sockaddr *)&client_address,
		                            &address_length, SOCK_NONBLOCK);
		if (unlikely(client_socket < 0)) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) return;

			panic("accept4: %s", strerror(errno));
		}

		uint64_t request_arrival_timestamp = __getcycles();

		http_total_increment_request();

		/* Allocate HTTP Session */
		struct http_session *session = http_session_alloc(client_socket,
		                                                  (const struct sockaddr *)&client_address, tenant,
		                                                  request_arrival_timestamp);

		/* Receive HTTP Request */
		int rc = http_session_receive_request(session, listener_thread_register_http_session);
		if (rc == -3) {
			continue;
		} else if (rc == -2) {
			debuglog("Request size exceeded Buffer\n");
			http_session_send_err_oneshot(session, 413);
			continue;
		} else if (rc == -1) {
			http_session_send_err_oneshot(session, 400);
			continue;
		}

		assert(session->http_request.message_end);
		assert(session->http_request.body);

		/* Route to sandbox */
		struct route *route = http_router_match_route(&tenant->router, session->http_request.full_url);
		if (route == NULL) {
			http_session_send_err_oneshot(session, 404);
			continue;
		}

		/*
		 * Perform admissions control.
		 * If 0, workload was rejected, so close with 429 "Too Many Requests" and continue
		 * TODO: Consider providing a Retry-After header
		 */
		uint64_t work_admitted = admissions_control_decide(route->admissions_info.estimate);
		if (work_admitted == 0) {
			http_session_send_err_oneshot(session, 429);
			continue;
		}

		/* Allocate a Sandbox */
		struct sandbox *sandbox = sandbox_alloc(route->module, session, route, tenant, work_admitted);
		if (unlikely(sandbox == NULL)) {
			http_session_send_err_oneshot(session, 503);
			continue;
		}

		/* If the global request scheduler is full, return a 429 to the client
		 */
		sandbox = global_request_scheduler_add(sandbox);
		if (unlikely(sandbox == NULL)) {
			http_session_send_err_oneshot(session, 429);
			sandbox_free(sandbox);
			continue;
		}
	}
}

static void
resume_blocked_read(struct epoll_event *evt)
{
	assert((evt->events & EPOLLIN) == EPOLLIN);

	/* Unpack http session from epoll event */
	struct http_session *session = (struct http_session *)evt->data.ptr;
	assert(session);

	/* Read HTTP request */
	int rc = http_session_receive_request(session, listener_thread_register_http_session);
	if (rc == -3) {
		return;
	} else if (rc == -2) {
		debuglog("Request size exceeded Buffer\n");
		/* Request size exceeded Buffer, send 413 Payload Too Large */
		listener_thread_unregister_http_session(session);
		http_session_send_err_oneshot(session, 413);
		return;
	} else if (rc == -1) {
		listener_thread_unregister_http_session(session);
		http_session_send_err_oneshot(session, 400);
		return;
	}

	assert(session->http_request.message_end);

	/* We read session to completion, so can remove from epoll */
	listener_thread_unregister_http_session(session);

	struct route *route = http_router_match_route(&session->tenant->router, session->http_request.full_url);
	if (route == NULL) {
		http_session_send_err_oneshot(session, 404);
		return;
	}

	/*
	 * Perform admissions control.
	 * If 0, workload was rejected, so close with 429 "Too Many Requests" and continue
	 * TODO: Consider providing a Retry-After header
	 */
	uint64_t work_admitted = admissions_control_decide(route->admissions_info.estimate);
	if (work_admitted == 0) {
		http_session_send_err_oneshot(session, 429);
		return;
	}

	/* Allocate a Sandbox */
	struct sandbox *sandbox = sandbox_alloc(route->module, session, route, session->tenant, work_admitted);
	if (unlikely(sandbox == NULL)) {
		http_session_send_err_oneshot(session, 503);
		return;
	}

	/* If the global request scheduler is full, return a 429 to the client */
	if (unlikely(global_request_scheduler_add(sandbox) == NULL)) {
		http_session_send_err_oneshot(session, 429);
		sandbox_free(sandbox);
	}
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
		/* Block indefinitely on the epoll file descriptor, waiting on up to a max number of events */
		int descriptor_count = epoll_wait(listener_thread_epoll_file_descriptor, epoll_events,
		                                  RUNTIME_MAX_EPOLL_EVENTS, -1);
		if (descriptor_count < 0) {
			if (errno == EINTR) continue;

			panic("epoll_wait: %s", strerror(errno));
		}

		/* Assumption: Because epoll_wait is set to not timeout, we should always have descriptors here */
		assert(descriptor_count > 0);

		for (int i = 0; i < descriptor_count; i++) {
			panic_on_epoll_error(&epoll_events[i]);

			if (tenant_database_find_by_ptr(epoll_events[i].data.ptr) != NULL) {
				handle_tcp_requests(&epoll_events[i]);
			} else {
				resume_blocked_read(&epoll_events[i]);
			}
		}
		generic_thread_dump_lock_overhead();
	}

	panic("Listener thread unexpectedly broke loop\n");
}
