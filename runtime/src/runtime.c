#include <signal.h>
#include <sched.h>
#include <sys/mman.h>
#include <uv.h>

#include "arch/context.h"
#include "debuglog.h"
#include "global_request_scheduler_deque.h"
#include "global_request_scheduler_minheap.h"
#include "http_parser_settings.h"
#include "http_response.h"
#include "module.h"
#include "runtime.h"
#include "sandbox_request.h"
#include "software_interrupt.h"

/***************************
 * Shared Process State    *
 **************************/

int    runtime_epoll_file_descriptor;
double runtime_admitted;

#ifdef LOG_TOTAL_REQS_RESPS
_Atomic uint32_t runtime_total_requests;
_Atomic uint32_t runtime_total_2XX_responses;
_Atomic uint32_t runtime_total_4XX_responses;
_Atomic uint32_t runtime_total_5XX_responses;
#endif

/******************************************
 * Shared Process / Listener Thread Logic *
 *****************************************/

/**
 * Initialize runtime global state, mask signals, and init http parser
 */
void
runtime_initialize(void)
{
	/* Setup epoll */
	runtime_epoll_file_descriptor = epoll_create1(0);
	assert(runtime_epoll_file_descriptor >= 0);


#ifdef LOG_TOTAL_REQS_RESPS
	/* Setup Counts */
	runtime_total_requests      = 0;
	runtime_total_2XX_responses = 0;
	runtime_total_4XX_responses = 0;
	runtime_total_5XX_responses = 0;
#endif

	/* Allocate and Initialize the global deque
	TODO: Improve to expose variant as a config #Issue 93
	*/
	// global_request_scheduler_deque_initialize();
	global_request_scheduler_minheap_initialize();

	/* Mask Signals */
	software_interrupt_mask_signal(SIGUSR1);
	software_interrupt_mask_signal(SIGALRM);

	/* Initialize http_parser_settings global */
	http_parser_settings_initialize();

	runtime_admitted = 0;
}

/*************************
 * Listener Thread Logic *
 ************************/

/**
 * Rejects Requests as determined by admissions control
 * @param client_socket - the client we are rejecting
 */
static inline void
listener_thread_reject(int client_socket)
{
	assert(client_socket >= 0);

	int send_rc = send(client_socket, HTTP_RESPONSE_504_SERVICE_UNAVAILABLE,
	                   strlen(HTTP_RESPONSE_504_SERVICE_UNAVAILABLE), 0);
	if (send_rc < 0) goto send_504_err;

#ifdef LOG_TOTAL_REQS_RESPS
	runtime_total_5XX_responses++;
	runtime_log_requests_responses();
#endif

close:
	if (close(client_socket) < 0) panic("Error closing client socket - %s", strerror(errno));
	return;
send_504_err:
	debuglog("Error sending 504: %s", strerror(errno));
	goto close;
}

/**
 * @brief Execution Loop of the listener core, io_handles HTTP requests, allocates sandbox request objects, and pushes
 * the sandbox object to the global dequeue
 * @param dummy data pointer provided by pthreads API. Unused in this function
 * @return NULL
 *
 * Used Globals:
 * runtime_epoll_file_descriptor - the epoll file descriptor
 *
 */
__attribute__((noreturn)) void *
listener_thread_main(void *dummy)
{
	struct epoll_event epoll_events[LISTENER_THREAD_MAX_EPOLL_EVENTS];

	while (true) {
		/*
		 * Block indefinitely on the epoll file descriptor, waiting on up to a max number of events
		 * TODO: Is LISTENER_THREAD_MAX_EPOLL_EVENTS actually limited to the max number of modules?
		 */
		int request_count = epoll_wait(runtime_epoll_file_descriptor, (struct epoll_event *)&epoll_events,
		                               LISTENER_THREAD_MAX_EPOLL_EVENTS, -1);
		if (request_count < 0) panic("epoll_wait: %s", strerror(errno));
		if (request_count == 0) panic("Unexpectedly returned with epoll_wait timeout not set\n");

		/* Capture Start Time to calculate absolute deadline */
		uint64_t request_arrival_timestamp = __getcycles();
		for (int i = 0; i < request_count; i++) {
			if (epoll_events[i].events & EPOLLERR) panic("epoll_wait: %s", strerror(errno));

			/* Unpack module from epoll event */
			struct module *module = (struct module *)epoll_events[i].data.ptr;
			assert(module);

			/* Accept Client Request as a nonblocking socket, saving address information */
			struct sockaddr_in client_address;
			socklen_t          address_length = sizeof(client_address);
			int client_socket = accept4(module->socket_descriptor, (struct sockaddr *)&client_address,
			                            &address_length, SOCK_NONBLOCK);
			if (client_socket < 0) {
				switch (errno) {
					/* Note: Assumes EAGAIN and EWOULDBLOCK are identical, as on Linux */
				case EWOULDBLOCK: {
					/*
					 * According to accept(2), it is possible that a connection might
					 * have been removed between us receiving an event via epoll_wait
					 * and us calling accept. Thus we just want to gracefully ignore the
					 * epoll event.
					 */

#ifdef LOG_EPOLL
					debuglog("Encountered an epoll notification for %s that did not actually have "
					         "an associated request\n",
					         module->name);
#endif

					continue;
				}
				default:
					panic("accept4: %s", strerror(errno));
				}
			};

			/*
			 * According to accept(2), it is possible that the the sockaddr structure client_address may be
			 * too small, resulting in data being truncated to fit. The appect call mutates the size value
			 * to indicate that this is the case.
			 */
			if (address_length > sizeof(client_address)) {
				debuglog("A client address to %s has been truncated because buffer was too small\n",
				         module->name);
			}

#ifdef LOG_TOTAL_REQS_RESPS
			runtime_total_requests++;
			runtime_log_requests_responses();
#endif

			/* Perform Admission Control */

			uint32_t estimated_execution = perf_window_get_percentile(&module->perf_window, 0.5);
			/*
			 * If this is the first execution, assume a default execution
			 * TODO: Enhance module specification to provide "seed" value of estimated duration
			 */
			if (estimated_execution == -1) estimated_execution = 1000;

			double admissions_estimate = (double)estimated_execution / module->relative_deadline;

			if (runtime_admitted + admissions_estimate >= runtime_worker_threads_count) {
				listener_thread_reject(client_socket);
				continue;
			}

			/* Allocate a Sandbox Request */
			struct sandbox_request *sandbox_request =
			  sandbox_request_allocate(module, module->name, client_socket,
			                           (const struct sockaddr *)&client_address, request_arrival_timestamp,
			                           admissions_estimate);

			/* Add to the Global Sandbox Request Scheduler */
			global_request_scheduler_add(sandbox_request);

			/* Add to work accepted by the runtime */
			runtime_admitted += admissions_estimate;

#ifdef LOG_ADMISSIONS_CONTROL
			debuglog("Runtime Admitted: %f / %u\n", runtime_admitted, runtime_worker_threads_count);
#endif
		}
	}

	panic("Listener thread unexpectedly broke loop\n");
}

/**
 * Initializes the listener thread, pinned to core 0, and starts to listen for requests
 */
void
listener_thread_initialize(void)
{
	cpu_set_t cs;

	CPU_ZERO(&cs);
	CPU_SET(LISTENER_THREAD_CORE_ID, &cs);

	pthread_t listener_thread;
	int       ret = pthread_create(&listener_thread, NULL, listener_thread_main, NULL);
	assert(ret == 0);
	ret = pthread_setaffinity_np(listener_thread, sizeof(cpu_set_t), &cs);
	assert(ret == 0);
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cs);
	assert(ret == 0);

	software_interrupt_initialize();
	software_interrupt_arm_timer();
}
