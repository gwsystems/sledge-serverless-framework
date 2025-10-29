#pragma once

#include "sandbox_types.h"

void local_cleanup_queue_add(struct sandbox *sandbox);
int  local_cleanup_queue_free(uint64_t *duration, uint64_t *ret);
void local_cleanup_queue_initialize();
