#pragma once

#include <stdbool.h>
#include <stdnoreturn.h>

#include "http_session.h"
#include "module.h"

#define LISTENER_THREAD_CORE_ID 1
#define DIPATCH_ROUNTE_ERROR "Did not match any routes"
#define WORK_ADMITTED_ERROR "Work is not admitted"
#define SANDBOX_ALLOCATION_ERROR "Failed to allocate a sandbox"
#define GLOBAL_QUEUE_ERROR "Failed to add sandbox to global queue" 

extern pthread_t listener_thread_id;

void           listener_thread_initialize(void);
noreturn void *listener_thread_main(void *dummy);
void           listener_thread_register_http_session(struct http_session *http);
void 	       dispatcher_send_response(void *req_handle, char* msg, size_t msg_len);

/**
 * Used to determine if running in the context of a listener thread
 * @returns true if listener. false if not (probably a worker)
 */
static inline bool
listener_thread_is_running()
{
	return pthread_self() == listener_thread_id;
}
