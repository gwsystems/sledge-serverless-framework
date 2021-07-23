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

static inline void
sandbox_remove_from_epoll(struct sandbox *sandbox)
{
        assert(sandbox != NULL);

        int rc = epoll_ctl(worker_thread_epoll_file_descriptor, EPOLL_CTL_DEL, sandbox->client_socket_descriptor, NULL);
        if (unlikely(rc < 0)) panic_err();

}

/**
 * Initializes a sandbox fd ready for use with the proper preopen magic
 * @param sandbox
 * @return index of handle we preopened or -1 on error (sandbox is null or all io_handles are exhausted)
 */
static inline int
sandbox_initialize_file_descriptor(struct sandbox *sandbox)
{
	if (!sandbox) return -1;
	int sandbox_fd;
	for (sandbox_fd = 0; sandbox_fd < SANDBOX_MAX_FD_COUNT; sandbox_fd++) {
		if (sandbox->file_descriptors[sandbox_fd] < 0) break;
	}
	if (sandbox_fd == SANDBOX_MAX_FD_COUNT) return -1;
	sandbox->file_descriptors[sandbox_fd] = SANDBOX_FILE_DESCRIPTOR_PREOPEN_MAGIC;
	return sandbox_fd;
}

/**
 * Free Linear Memory, leaving stack in place
 * @param sandbox
 */
static inline void
sandbox_free_linear_memory(struct sandbox *sandbox)
{
	int rc = munmap(sandbox->linear_memory_start, SANDBOX_MAX_MEMORY + PAGE_SIZE);
	if (rc == -1) panic("sandbox_free_linear_memory - munmap failed\n");
	sandbox->linear_memory_start = NULL;
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

/**
 * Resolve a sandbox's fd to the host fd it maps to
 * @param sandbox
 * @param sandbox_fd index into the sandbox's fd table
 * @returns file descriptor or -1 in case of error
 */
static inline int
sandbox_get_file_descriptor(struct sandbox *sandbox, int sandbox_fd)
{
	if (!sandbox) return -1;
	if (sandbox_fd >= SANDBOX_MAX_FD_COUNT || sandbox_fd < 0) return -1;
	return sandbox->file_descriptors[sandbox_fd];
}

static inline uint64_t
sandbox_get_priority(void *element)
{
	struct sandbox *sandbox = (struct sandbox *)element;
	return sandbox->absolute_deadline;
};

/**
 * Maps a sandbox fd to an underlying host fd
 * Returns error condition if the file_descriptor to set does not contain sandbox preopen magic
 * @param sandbox
 * @param sandbox_fd index of the sandbox fd we want to set
 * @param file_descriptor the file descripter we want to set it to
 * @returns the index that was set or -1 in case of error
 */
static inline int
sandbox_set_file_descriptor(struct sandbox *sandbox, int sandbox_fd, int host_fd)
{
	if (!sandbox) return -1;
	if (sandbox_fd >= SANDBOX_MAX_FD_COUNT || sandbox_fd < 0) return -1;
	if (host_fd < 0 || sandbox->file_descriptors[sandbox_fd] != SANDBOX_FILE_DESCRIPTOR_PREOPEN_MAGIC) return -1;
	sandbox->file_descriptors[sandbox_fd] = host_fd;
	return sandbox_fd;
}

/**
 * Map the host stdin, stdout, stderr to the sandbox
 * @param sandbox - the sandbox on which we are initializing stdio
 */
static inline void
sandbox_initialize_stdio(struct sandbox *sandbox)
{
	int sandbox_fd, rc;
	for (int host_fd = 0; host_fd <= 2; host_fd++) {
		sandbox_fd = sandbox_initialize_file_descriptor(sandbox);
		assert(sandbox_fd == host_fd);
		rc = sandbox_set_file_descriptor(sandbox, sandbox_fd, host_fd);
		assert(rc != -1);
	}
}

static inline void
sandbox_open_http(struct sandbox *sandbox)
{
	assert(sandbox != NULL);

	http_parser_init(&sandbox->http_parser, HTTP_REQUEST);

	/* Set the sandbox as the data the http-parser has access to */
	sandbox->http_parser.data = sandbox; //assign data to sandbox in case to operator it when a callback happended

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

	uint32_t total_time_us = sandbox->total_time / runtime_processor_speed_MHz;
	uint32_t queued_us     = (sandbox->allocation_timestamp - sandbox->enqueue_timestamp)
	                     / runtime_processor_speed_MHz;
	uint32_t initializing_us = sandbox->initializing_duration / runtime_processor_speed_MHz;
	uint32_t runnable_us     = sandbox->runnable_duration / runtime_processor_speed_MHz;
	uint32_t running_us      = sandbox->running_duration / runtime_processor_speed_MHz;
	uint32_t blocked_us      = sandbox->blocked_duration / runtime_processor_speed_MHz;
	uint32_t returned_us     = sandbox->returned_duration / runtime_processor_speed_MHz;

	/*
	 * Assumption: A sandbox is never able to free pages. If linear memory management
	 * becomes more intelligent, then peak linear memory size needs to be tracked
	 * seperately from current linear memory size.
	 */
	fprintf(runtime_sandbox_perf_log, "%lu,%s():%d,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", sandbox->id,
	        sandbox->module->name, sandbox->module->port, sandbox_state_stringify(sandbox->state),
	        sandbox->module->relative_deadline_us, total_time_us, queued_us, initializing_us, runnable_us,
	        running_us, blocked_us, returned_us, sandbox->linear_memory_size);
}
