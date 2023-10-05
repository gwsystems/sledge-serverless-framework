#pragma once
#include <stdint.h> 
#include <stddef.h>

#define RQUEUE_LEN 4096
#define MAX_WORKERS 32

extern uint32_t runtime_worker_group_size;

struct request_msg {
	uint8_t *msg;
	size_t   size;
};

struct request_typed_queue {
    uint8_t type;
    uint64_t mean_ns;
    uint64_t deadline;
    double ratio;
    struct sandbox *rqueue[RQUEUE_LEN];
    unsigned int rqueue_tail;
    unsigned int rqueue_head;
    uint64_t tsqueue[RQUEUE_LEN];

    // Profiling variables
    uint64_t windows_mean_ns;
    uint64_t windows_count;
    uint64_t delay;

    //DARC variables
    uint32_t res_workers[MAX_WORKERS]; //record the worker id from 0 to the maximum id for a listener, 
				       //not the true worker id. It's value is the index of worker_list
    uint32_t n_resas;
    uint64_t last_resa;
    uint32_t stealable_workers[MAX_WORKERS];
    uint32_t n_stealable;
    int type_group;
    uint64_t max_delay;
    double prev_demand;

};

/*
 * n_resas the number of reserved workers. Leaving the last n_resas workers as the reserved workers 
 */
struct request_typed_queue * request_typed_queue_init(uint8_t type, uint32_t n_resas);

int push_to_rqueue(struct sandbox *sandbox, struct request_typed_queue *rtype, uint64_t tsc);
