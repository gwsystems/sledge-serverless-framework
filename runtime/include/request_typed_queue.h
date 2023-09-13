#pragma once

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

    /*RequestType(enum ReqType type, uint64_t mean_ns, uint64_t deadline, double ratio) :
        type(type), mean_ns(mean_ns), deadline(deadline), ratio(ratio) {
            //There must be a way to 0-init an C array from initializer list
            memset(rqueue, 0, RQUEUE_LEN * sizeof(unsigned long));
            rqueue_tail = 0;
            rqueue_head = 0;
        };
    */
};

/*
 * n_resas the number of reserved workers. Leaving the last n_resas workers as the reserved workers 
 */
static inline struct request_typed_queue *
request_typed_queue_init(uint8_t type, uint32_t n_resas) {
	struct request_typed_queue *queue = malloc(sizeof(struct request_typed_queue));
	queue->type = type;
	queue->mean_ns = 0;
	queue->deadline = 0;
	queue->rqueue_tail = 0;
	queue->rqueue_head = 0;
	queue->n_resas = n_resas;
	for (unsigned int i = 0; i < n_resas; ++i) {
		queue->res_workers[i] = i;
	}

	queue->n_stealable = runtime_worker_group_size - n_resas;
	int index = 0;
	for (unsigned int i = n_resas; i < runtime_worker_group_size; i++) {
		queue->stealable_workers[index] = i;
		index++;
	}
 
	memset(queue->rqueue, 0, RQUEUE_LEN * sizeof(struct sandbox*));
	
	return queue;	

}

static inline int push_to_rqueue(struct sandbox *sandbox, struct request_typed_queue *rtype, uint64_t tsc) {
    if (unlikely(rtype->rqueue_head - rtype->rqueue_tail == RQUEUE_LEN)) {
        panic("Dispatcher dropped request as type %hhu because queue is full\n", rtype->type);
        return -1;
    } else {
        //PSP_DEBUG("Pushed one request to queue " << req_type_str[static_cast<int>(rtype.type)]);
        rtype->tsqueue[rtype->rqueue_head & (RQUEUE_LEN - 1)] = tsc;
        rtype->rqueue[rtype->rqueue_head++ & (RQUEUE_LEN - 1)] = sandbox;
        return 0;
    }
}

