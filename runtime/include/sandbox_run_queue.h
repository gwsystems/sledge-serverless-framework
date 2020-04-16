#ifndef SFRT_SANDBOX_RUN_QUEUE_H
#define SFRT_SANDBOX_RUN_QUEUE_H

#include <stdbool.h>

#include "sandbox.h"

void sandbox_run_queue_initialize();

bool sandbox_run_queue_is_empty();

// Get the sandbox at the head of the thread local runqueue
struct sandbox *sandbox_run_queue_get_head();

// Remove a sandbox from the runqueue
void sandbox_run_queue_remove(struct sandbox *sandbox_to_remove);

/**
 * Append the sandbox to the worker_thread_run_queue
 * @param sandbox_to_append
 */
void sandbox_run_queue_append(struct sandbox *sandbox_to_append);

#endif /* SFRT_SANDBOX_RUN_QUEUE_H */