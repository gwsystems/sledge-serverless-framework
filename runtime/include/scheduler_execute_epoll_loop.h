#pragma once

#include <assert.h>
#include <errno.h>

#include "http_session.h"
#include "panic.h"
#include "runtime.h"
#include "sandbox_functions.h"
#include "sandbox_set_as_error.h"
#include "sandbox_state.h"
#include "sandbox_types.h"
#include "worker_thread.h"


/**
 * Run all outstanding events in the local thread's epoll loop
 */
static inline void
scheduler_execute_epoll_loop(void)
{
	while (true) {
		struct epoll_event epoll_events[RUNTIME_MAX_EPOLL_EVENTS];
		int                descriptor_count = epoll_wait(worker_thread_epoll_file_descriptor, epoll_events,
		                                                 RUNTIME_MAX_EPOLL_EVENTS, 0);

		if (descriptor_count < 0) {
			if (errno == EINTR) continue;

			panic_err();
		}

		if (descriptor_count == 0) break;

		for (int i = 0; i < descriptor_count; i++) {
			if (epoll_events[i].events & (EPOLLIN | EPOLLOUT)) {
				/* Re-add to runqueue if asleep */
				struct sandbox *sandbox = (struct sandbox *)epoll_events[i].data.ptr;
				assert(sandbox);

				if (sandbox->state == SANDBOX_ASLEEP) { sandbox_wakeup(sandbox); }
			} else if (epoll_events[i].events & (EPOLLERR | EPOLLHUP)) {
				/* Close socket and set as error on socket error or unexpected client hangup */
				struct sandbox *sandbox = (struct sandbox *)epoll_events[i].data.ptr;
				int             error   = 0;
				socklen_t       errlen  = sizeof(error);
				getsockopt(epoll_events[i].data.fd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen);

				if (error > 0) {
					debuglog("Socket error: %s", strerror(error));
				} else if (epoll_events[i].events & EPOLLHUP) {
					debuglog("Client Hungup");
				} else {
					debuglog("Unknown Socket error");
				}

				switch (sandbox->state) {
				case SANDBOX_RETURNED:
				case SANDBOX_COMPLETE:
				case SANDBOX_ERROR:
					panic("Expected to have closed socket");
				default:
					http_session_send_err_oneshot(sandbox->http, 503);
					http_session_close(sandbox->http);
					sandbox_set_as_error(sandbox, sandbox->state);
				}
			} else {
				panic("Mystery epoll event!\n");
			};
		}
	}
}
