#pragma once

#include "sandbox_types.h"

void local_cleanup_queue_add(struct sandbox *sandbox);
void local_cleanup_queue_free();
void local_cleanup_queue_initialize();
