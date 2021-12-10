#pragma once

#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>

#include "client_socket.h"
#include "panic.h"
#include "sandbox_types.h"

/***************************
 * Public API              *
 **************************/

struct sandbox *sandbox_new(struct module *module, int socket_descriptor, const struct sockaddr *socket_address,
                            uint64_t request_arrival_timestamp, uint64_t admissions_estimate);
int             sandbox_prepare_execution_environment(struct sandbox *sandbox);
void            sandbox_free(struct sandbox *sandbox);
void            sandbox_main(struct sandbox *sandbox);
void            sandbox_switch_to(struct sandbox *next_sandbox);
static inline void
sandbox_close_http(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	int rc = epoll_ctl(worker_thread_epoll_file_descriptor, EPOLL_CTL_DEL, sandbox->client_socket_descriptor, NULL);
	if (unlikely(rc < 0)) panic_err();

	client_socket_close(sandbox->client_socket_descriptor, &sandbox->client_address);
}

/**
 * Free Linear Memory, leaving stack in place
 * @param sandbox
 */
static inline void
sandbox_free_linear_memory(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	assert(sandbox->memory != NULL);
	module_free_linear_memory(sandbox->module, sandbox->memory);
	sandbox->memory = NULL;
}

/**
 * Free Linear Memory, leaving stack in place
 * @param sandbox
 */
static inline void
sandbox_free_http_buffers(struct sandbox *sandbox)
{
	assert(sandbox);
	vec_u8_deinit(&sandbox->request);
	vec_u8_deinit(&sandbox->response);
}

/**
 * Given a sandbox, returns the module that sandbox is executing
 * @param sandbox the sandbox whose module we want
 * @return the module of the provided sandbox
 */
static inline struct module *
sandbox_get_module(struct sandbox *sandbox)
{
	if (!sandbox) return NULL;
	return sandbox->module;
}

static inline uint64_t
sandbox_get_priority(void *element)
{
	struct sandbox *sandbox = (struct sandbox *)element;
	return sandbox->absolute_deadline;
};

static inline void
sandbox_open_http(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	http_parser_init(&sandbox->http_parser, HTTP_REQUEST);

	/* Set the sandbox as the data the http-parser has access to */
	sandbox->http_parser.data = sandbox;

	/* Freshly allocated sandbox going runnable for first time, so register client socket with epoll */
	struct epoll_event accept_evt;
	accept_evt.data.ptr = (void *)sandbox;
	accept_evt.events   = EPOLLIN | EPOLLOUT | EPOLLET;
	int rc = epoll_ctl(worker_thread_epoll_file_descriptor, EPOLL_CTL_ADD, sandbox->client_socket_descriptor,
	                   &accept_evt);
	if (unlikely(rc < 0)) panic_err();
}
