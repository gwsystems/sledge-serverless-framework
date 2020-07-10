#include <pthread.h>
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
#include "types.h"

/***************************
 * Shared Process State    *
 **************************/

int runtime_epoll_file_descriptor;

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
	TODO: Improve to expose variant as a config
	*/
	// global_request_scheduler_deque_initialize();
	global_request_scheduler_minheap_initialize();

	/* Mask Signals */
	software_interrupt_mask_signal(SIGUSR1);
	software_interrupt_mask_signal(SIGALRM);

	/* Initialize http_parser_settings global */
	http_parser_settings_initialize();
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
		uint64_t start_time = __getcycles();
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

			/* Allocate a Sandbox Request */
			struct sandbox_request *sandbox_request =
			  sandbox_request_allocate(module, module->name, socket_descriptor,
			                           (const struct sockaddr *)&client_address, start_time);
			assert(sandbox_request);

			/* Add to the Global Sandbox Request Scheduler */
			global_request_scheduler_add(sandbox_request);
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
