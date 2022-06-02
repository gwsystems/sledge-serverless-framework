#pragma once

#include "worker_thread.h"

static inline void
worker_thread_epoll_add_sandbox(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	/* Freshly allocated sandbox going runnable for first time, so register client socket with epoll */
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)sandbox;
	accept_evt.events   = EPOLLIN | EPOLLOUT | EPOLLET;
	int rc = epoll_ctl(worker_thread_epoll_file_descriptor, EPOLL_CTL_ADD, sandbox->http->socket, &accept_evt);
	if (unlikely(rc < 0)) panic_err();
}

static inline void
worker_thread_epoll_remove_sandbox(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	int rc = epoll_ctl(worker_thread_epoll_file_descriptor, EPOLL_CTL_DEL, sandbox->http->socket, NULL);
	if (unlikely(rc < 0)) panic_err();
}
