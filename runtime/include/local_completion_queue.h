#pragma once

#include "ps_list.h"
#include "sandbox_types.h"

void local_completion_queue_add(struct sandbox *sandbox);
void local_completion_queue_free();
void local_completion_queue_initialize();
