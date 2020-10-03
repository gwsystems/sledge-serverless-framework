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

int              runtime_epoll_file_descriptor;
_Atomic uint64_t runtime_admitted;
uint64_t         runtime_admissions_capacity;

#ifdef LOG_TOTAL_REQS_RESPS
_Atomic uint32_t runtime_total_requests      = 0;
_Atomic uint32_t runtime_total_2XX_responses = 0;
_Atomic uint32_t runtime_total_4XX_responses = 0;
_Atomic uint32_t runtime_total_5XX_responses = 0;

void
runtime_log_requests_responses()
{
	uint32_t total_reqs = atomic_load(&runtime_total_requests);
	uint32_t total_2XX  = atomic_load(&runtime_total_2XX_responses);
	uint32_t total_4XX  = atomic_load(&runtime_total_4XX_responses);
	uint32_t total_5XX  = atomic_load(&runtime_total_5XX_responses);

	int64_t total_responses      = total_2XX + total_4XX + total_5XX;
	int64_t outstanding_requests = (int64_t)total_reqs - total_responses;

	debuglog("Requests: %u (%ld outstanding)\n\tResponses: %ld\n\t\t2XX: %u\n\t\t4XX: %u\n\t\t5XX: %u\n",
	         total_reqs, outstanding_requests, total_responses, total_2XX, total_4XX, total_5XX);
};
#endif

#ifdef LOG_SANDBOX_TOTALS
_Atomic uint32_t runtime_total_freed_requests        = 0;
_Atomic uint32_t runtime_total_initialized_sandboxes = 0;
_Atomic uint32_t runtime_total_runnable_sandboxes    = 0;
_Atomic uint32_t runtime_total_blocked_sandboxes     = 0;
_Atomic uint32_t runtime_total_running_sandboxes     = 0;
_Atomic uint32_t runtime_total_preempted_sandboxes   = 0;
_Atomic uint32_t runtime_total_returned_sandboxes    = 0;
_Atomic uint32_t runtime_total_error_sandboxes       = 0;
_Atomic uint32_t runtime_total_complete_sandboxes    = 0;

/*
 * Function intended to be interactively run in a debugger to look at sandbox totals
 * via `call runtime_log_sandbox_states()`
 */
void
runtime_log_sandbox_states()
{
	uint32_t total_initialized = atomic_load(&runtime_total_initialized_sandboxes);
	uint32_t total_runnable    = atomic_load(&runtime_total_runnable_sandboxes);
	uint32_t total_blocked     = atomic_load(&runtime_total_blocked_sandboxes);
	uint32_t total_running     = atomic_load(&runtime_total_running_sandboxes);
	uint32_t total_preempted   = atomic_load(&runtime_total_preempted_sandboxes);
	uint32_t total_returned    = atomic_load(&runtime_total_returned_sandboxes);
	uint32_t total_error       = atomic_load(&runtime_total_error_sandboxes);
	uint32_t total_complete    = atomic_load(&runtime_total_complete_sandboxes);


	debuglog("Initialized: %u\n\tRunnable: %u\n\tBlocked: %u\n\tRunning: %u\n\tPreempted: %u\n\tReturned: "
	         "%u\n\tError: %u\n\tComplete: %u\n",
	         total_initialized, total_runnable, total_blocked, total_running, total_preempted, total_returned,
	         total_error, total_complete);
};
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
#ifdef LOG_TOTAL_REQS_RESPS
	atomic_init(&runtime_total_requests, 0);
	atomic_init(&runtime_total_2XX_responses, 0);
	atomic_init(&runtime_total_4XX_responses, 0);
	atomic_init(&runtime_total_5XX_responses, 0);
#endif
#ifdef LOG_SANDBOX_TOTALS
	atomic_init(&runtime_total_freed_requests, 0);
	atomic_init(&runtime_total_initialized_sandboxes, 0);
	atomic_init(&runtime_total_runnable_sandboxes, 0);
	atomic_init(&runtime_total_blocked_sandboxes, 0);
	atomic_init(&runtime_total_running_sandboxes, 0);
	atomic_init(&runtime_total_preempted_sandboxes, 0);
	atomic_init(&runtime_total_returned_sandboxes, 0);
	atomic_init(&runtime_total_error_sandboxes, 0);
	atomic_init(&runtime_total_complete_sandboxes, 0);
#endif

	/* Setup epoll */
	runtime_epoll_file_descriptor = epoll_create1(0);
	assert(runtime_epoll_file_descriptor >= 0);

	/* Allocate and Initialize the global deque
	TODO: Improve to expose variant as a config #Issue 93
	*/
	// global_request_scheduler_deque_initialize();
	global_request_scheduler_minheap_initialize();

	/* Mask Signals */
	software_interrupt_mask_signal(SIGUSR1);
	software_interrupt_mask_signal(SIGALRM);
	signal(SIGPIPE, SIG_IGN);

	/* Initialize http_parser_settings global */
	http_parser_settings_initialize();

	/* Initialize admissions control state */
	runtime_admissions_capacity = runtime_worker_threads_count * RUNTIME_GRANULARITY;
	runtime_admitted            = 0;
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

	int rc;
	int sent    = 0;
	int to_send = strlen(HTTP_RESPONSE_504_SERVICE_UNAVAILABLE);

	while (sent < to_send) {
		rc = write(client_socket, &HTTP_RESPONSE_504_SERVICE_UNAVAILABLE[sent], to_send - sent);
		if (rc < 0) {
			if (errno == EAGAIN) continue;

			goto send_504_err;
		}
		sent += rc;
	};

#ifdef LOG_TOTAL_REQS_RESPS
	atomic_fetch_add(&runtime_total_5XX_responses, 1);
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
		int descriptor_count = epoll_wait(runtime_epoll_file_descriptor, epoll_events,
		                                  LISTENER_THREAD_MAX_EPOLL_EVENTS, -1);
		if (descriptor_count < 0) {
			if (errno == EINTR) continue;

			panic("epoll_wait: %s", strerror(errno));
		}
		/* Assumption: Because epoll_wait is set to not timeout, we should always have descriptors here */
		assert(descriptor_count > 0);

		/* Capture Start Time to calculate absolute deadline */
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

			/* Assumption: We have only registered EPOLLIN events, so we should see no others here */
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
			 * This inner loop is used in case there are more datagrams than epoll events for some reason
			 */
			while (true) {
				int client_socket = accept4(module->socket_descriptor,
				                            (struct sockaddr *)&client_address, &address_length,
				                            SOCK_NONBLOCK);
				if (client_socket < 0) {
					if (errno == EWOULDBLOCK || errno == EAGAIN) break;

					panic("accept4: %s", strerror(errno));
				}

				/*
				 * According to accept(2), it is possible that the the sockaddr structure client_address
				 * may be too small, resulting in data being truncated to fit. The appect call mutates
				 * the size value to indicate that this is the case.
				 */
				if (address_length > sizeof(client_address)) {
					debuglog("A client address to %s has been truncated because buffer was too "
					         "small\n",
					         module->name);
				}

#ifdef LOG_TOTAL_REQS_RESPS
				atomic_fetch_add(&runtime_total_requests, 1);
#endif

				/* Perform Admission Control */

				uint32_t estimated_execution = perf_window_get_percentile(&module->perf_window, 0.5);
				/*
				 * If this is the first execution, assume a default execution
				 * TODO: Enhance module specification to provide "seed" value of estimated duration
				 */
				if (estimated_execution == -1) estimated_execution = 1000;

				uint64_t admissions_estimate = (((uint64_t)estimated_execution) * RUNTIME_GRANULARITY)
				                               / module->relative_deadline;

				if (runtime_admitted + admissions_estimate >= runtime_admissions_capacity) {
					listener_thread_reject(client_socket);
					continue;
				}

				/* Allocate a Sandbox Request */
				struct sandbox_request *sandbox_request =
				  sandbox_request_allocate(module, module->name, client_socket,
				                           (const struct sockaddr *)&client_address,
				                           request_arrival_timestamp, admissions_estimate);

				/* Add to the Global Sandbox Request Scheduler */
				global_request_scheduler_add(sandbox_request);

				/* Add to work accepted by the runtime */
				runtime_admitted += admissions_estimate;

#ifdef LOG_ADMISSIONS_CONTROL
				debuglog("Runtime Admitted: %lu / %lu\n", runtime_admitted,
				         runtime_admissions_capacity);
#endif
			} /* while true */
		}         /* for loop */
	}                 /* while true */

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
