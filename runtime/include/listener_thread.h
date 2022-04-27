#pragma once

#include <stdbool.h>
#include <stdnoreturn.h>

#include "generic_thread.h"
#include "module.h"

#define LISTENER_THREAD_CORE_ID 1

extern pthread_t listener_thread_id;

void           listener_thread_initialize(void);
noreturn void *listener_thread_main(void *dummy);
int            listener_thread_register_module(struct module *mod);

/**
 * Used to determine if running in the context of a listener thread
 * @returns true if listener. false if not (probably a worker)
 */
static inline bool
listener_thread_is_running()
{
	return pthread_self() == listener_thread_id;
}
