#pragma once
#include <stdint.h> 
#include <stddef.h>

#define RQUEUE_QUEUE_LEN 256

struct request_fifo_queue {
    struct sandbox *rqueue[RQUEUE_QUEUE_LEN];
    unsigned int rqueue_tail;
    unsigned int rqueue_head;
    uint64_t tsqueue[RQUEUE_QUEUE_LEN];
};

