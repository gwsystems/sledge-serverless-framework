#pragma once

#include <threads.h>

#include "runtime.h"

extern thread_local struct arch_context worker_thread_base_context;
extern thread_local int                 worker_thread_idx;
extern thread_local void         *worker_dbf;

void *worker_thread_main(void *return_code);
