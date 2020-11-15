#include <signal.h>
#include <sched.h>
#include <sys/mman.h>

#include "admissions_control.h"
#include "arch/context.h"
#include "client_socket.h"
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
	http_total_init();
	sandbox_request_count_initialize();
	sandbox_count_initialize();

	/* Setup epoll */
	runtime_epoll_file_descriptor = epoll_create1(0);
	assert(runtime_epoll_file_descriptor >= 0);

	/* Setup Scheduler */
	switch (runtime_scheduler) {
	case RUNTIME_SCHEDULER_EDF:
		global_request_scheduler_minheap_initialize();
		break;
	case RUNTIME_SCHEDULER_FIFO:
		global_request_scheduler_deque_initialize();
		break;
	default:
		panic("Invalid scheduler policy set: %u\n", runtime_scheduler);
	}

	/* Mask Signals */
	software_interrupt_mask_signal(SIGUSR1);
	software_interrupt_mask_signal(SIGALRM);
	signal(SIGPIPE, SIG_IGN);

	http_parser_settings_initialize();
	admissions_control_initialize();
}

/*************************
 * Listener Thread Logic *
 ************************/

static inline void
listener_thread_start_lock_overhead_measurement(uint64_t request_arrival_timestamp)
{
#ifdef LOG_LISTENER_LOCK_OVERHEAD
	worker_thread_start_timestamp = request_arrival_timestamp;
	worker_thread_lock_duration   = 0;
#endif
}

static inline void
listener_thread_stop_lock_overhead_measurement()
{
#ifdef LOG_LISTENER_LOCK_OVERHEAD
	uint64_t worker_duration = __getcycles() - worker_thread_start_timestamp;
	debuglog("Locks consumed %lu / %lu cycles, or %f%%\n", worker_thread_lock_duration, worker_duration,
	         (double)worker_thread_lock_duration / worker_duration * 100);
#endif
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

		uint64_t request_arrival_timestamp = __getcycles();
		listener_thread_start_lock_overhead_measurement(request_arrival_timestamp);
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

				http_total_increment_request();

				/*
				 * Perform admissions control.
				 * If 0, workload was rejected, so close with 503 and continue
				 */
				uint64_t work_admitted = admissions_control_decide(module->admissions_info.estimate);
				if (work_admitted == 0) {
					client_socket_send(client_socket, 503);
					if (unlikely(close(client_socket) < 0))
						debuglog("Error closing client socket - %s", strerror(errno));

					continue;
				}

				/* Allocate a Sandbox Request */
				struct sandbox_request *sandbox_request =
				  sandbox_request_allocate(module, module->name, client_socket,
				                           (const struct sockaddr *)&client_address,
				                           request_arrival_timestamp, work_admitted);

				/* Add to the Global Sandbox Request Scheduler */
				global_request_scheduler_add(sandbox_request);

			} /* while true */
		}         /* for loop */
		listener_thread_stop_lock_overhead_measurement();
	} /* while true */

	panic("Listener thread unexpectedly broke loop\n");


	/* Cleanup Tasks... These won't run, but placed here to keep track */
	fclose(runtime_sandbox_perf_log);
}

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
