#pragma once

#include "sandbox.h"

void sandbox_completion_queue_add(struct sandbox *sandbox);
void sandbox_completion_queue_free();
void sandbox_completion_queue_initialize();
