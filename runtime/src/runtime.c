#include <signal.h>
#include <sched.h>
#include <sys/mman.h>
#include <uv.h>

#include "arch/context.h"
#include "global_request_scheduler_deque.h"
#include "global_request_scheduler_minheap.h"
#include "http_parser_settings.h"
#include "module.h"
#include "runtime.h"
#include "sandbox_request.h"
#include "software_interrupt.h"

/***************************
 * Shared Process State    *
 **************************/

int    runtime_epoll_file_descriptor;
double runtime_admitted;

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
 * @brief Execution Loop of the listener core, io_handles HTTP requests, allocates sandbox request objects, and pushes
 * the sandbox object to the global dequeue
 * @param dummy data pointer provided by pthreads API. Unused in this function
 * @return NULL
 *
 * Used Globals:
 * runtime_epoll_file_descriptor - the epoll file descriptor
 *
 */
void *
listener_thread_main(void *dummy)
{
	struct epoll_event *epoll_events   = (struct epoll_event *)malloc(LISTENER_THREAD_MAX_EPOLL_EVENTS
                                                                        * sizeof(struct epoll_event));
	int                 total_requests = 0;

	while (true) {
		int request_count = epoll_wait(runtime_epoll_file_descriptor, epoll_events,
		                               LISTENER_THREAD_MAX_EPOLL_EVENTS, -1);

		/* Capture Start Time to calculate absolute deadline */
		uint64_t request_arrival_timestamp = __getcycles();
		for (int i = 0; i < request_count; i++) {
			if (epoll_events[i].events & EPOLLERR) {
				perror("epoll_wait");
				assert(false);
			}

			/* Accept Client Request */
			struct sockaddr_in client_address;
			socklen_t          client_length = sizeof(client_address);
			struct module *    module        = (struct module *)epoll_events[i].data.ptr;
			assert(module);
			int es                = module->socket_descriptor;
			int socket_descriptor = accept(es, (struct sockaddr *)&client_address, &client_length);
			if (socket_descriptor < 0) {
				perror("accept");
				assert(false);
			}
			total_requests++;

			/* Perform Admission Control */

			/*
			 * TODO: Enhance to use configurable percentiles rather than just mean. This can be policy
			 * defined in the module specification
			 */
			uint64_t estimated_execution = perf_window_get_mean(&module->perf_window);

			/*
			 * If this is the first execution, assume a default execution
			 * TODO: Enhance module specification to provide "seed" value of estimated duration
			 * TODO: Should we "rate limit" or only admit one request before we have actual data? Otherwise
			 * we might be flooded with sandboxes that possibly underestimate
			 */
			if (estimated_execution == -1) estimated_execution = 1000;

			double admissions_estimate = (double)estimated_execution / module->relative_deadline;

			/*
			 * Reject Requests that exceed system capacity
			 * TODO: Enhance to gracefully return HTTP status code 503 Service Unavailable
			 */
			if (runtime_admitted + admissions_estimate >= runtime_worker_threads_count) {
				debuglog("Would have rejected!");
			}

			/* Allocate a Sandbox Request */
			struct sandbox_request *sandbox_request =
			  sandbox_request_allocate(module, module->name, socket_descriptor,
			                           (const struct sockaddr *)&client_address, request_arrival_timestamp,
			                           admissions_estimate);
			assert(sandbox_request);

			/* Add to the Global Sandbox Request Scheduler */
			global_request_scheduler_add(sandbox_request);

			/* Add to work accepted by the runtime */
			runtime_admitted += admissions_estimate;
			debuglog("Runtime Utilization: %f%%\n", runtime_admitted / runtime_worker_threads_count * 100);
		}
	}

	free(epoll_events);
	return NULL;
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
