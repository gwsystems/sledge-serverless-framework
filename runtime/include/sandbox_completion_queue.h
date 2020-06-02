#ifndef SFRT_SANDBOX_COMPLETION_QUEUE_H
#define SFRT_SANDBOX_COMPLETION_QUEUE_H

#include "sandbox.h"

void sandbox_completion_queue_add(struct sandbox *sandbox);
void sandbox_completion_queue_free();
void sandbox_completion_queue_initialize();

#endif /* SFRT_SANDBOX_COMPLETION_QUEUE_H */