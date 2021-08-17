#pragma once

#include <sys/mman.h>
#include <stddef.h>
#include <stdint.h>

#include "client_socket.h"
#include "panic.h"
#include "sandbox_request.h"

/***************************
 * Public API              *
 **************************/

struct sandbox *sandbox_allocate(struct sandbox_request *sandbox_request);
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
	int rc = munmap(sandbox->memory.start, SANDBOX_MAX_MEMORY + PAGE_SIZE);
	if (rc == -1) panic("sandbox_free_linear_memory - munmap failed\n");
	sandbox->memory.start = NULL;
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

/**
 * Prints key performance metrics for a sandbox to runtime_sandbox_perf_log
 * This is defined by an environment variable
 * @param sandbox
 */
static inline void
sandbox_print_perf(struct sandbox *sandbox)
{
	/* If the log was not defined by an environment variable, early out */
	if (runtime_sandbox_perf_log == NULL) return;

	uint64_t queued_duration = sandbox->timestamp_of.allocation - sandbox->timestamp_of.request_arrival;

	/*
	 * Assumption: A sandbox is never able to free pages. If linear memory management
	 * becomes more intelligent, then peak linear memory size needs to be tracked
	 * seperately from current linear memory size.
	 */
	fprintf(runtime_sandbox_perf_log, "%lu,%s,%d,%s,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%u,%u\n", sandbox->id,
	        sandbox->module->name, sandbox->module->port, sandbox_state_stringify(sandbox->state),
	        sandbox->module->relative_deadline, sandbox->total_time, queued_duration,
	        sandbox->duration_of_state.initializing, sandbox->duration_of_state.runnable,
	        sandbox->duration_of_state.running, sandbox->duration_of_state.blocked,
	        sandbox->duration_of_state.returned, runtime_processor_speed_MHz, sandbox->memory.size);
}
