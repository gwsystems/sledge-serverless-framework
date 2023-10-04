#pragma once

#include "request_fifo_queue.h"

void local_preempted_fifo_queue_init();
struct sandbox * pop_worker_preempted_queue(int worker_id, struct request_fifo_queue * queue, uint64_t * tsc);
int push_to_preempted_queue(struct sandbox *sandbox, uint64_t tsc);
