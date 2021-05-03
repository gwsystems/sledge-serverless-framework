#pragma once

#include "generic_thread.h"
#include "module.h"

#define LISTENER_THREAD_CORE_ID 0

void                            listener_thread_initialize(void);
__attribute__((noreturn)) void *listener_thread_main(void *dummy);
int                             listener_thread_register_module(struct module *mod);
