#pragma once

#include "sandbox.h"

void local_completion_queue_add(struct sandbox *sandbox);
void local_completion_queue_free();
void local_completion_queue_initialize();
