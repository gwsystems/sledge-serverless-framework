#pragma once

#include "deque.h"
#include "global_request_scheduler.h"
#include "sandbox_types.h"

DEQUE_PROTOTYPE(sandbox, struct sandbox *)

void global_request_scheduler_deque_initialize();
