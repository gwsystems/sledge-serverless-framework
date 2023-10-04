#include <malloc.h>
#include <string.h>

#include "panic.h"
#include "likely.h"
#include "request_typed_queue.h"

struct request_typed_queue *
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

int push_to_rqueue(uint8_t dispatcher_id, struct sandbox *sandbox, struct request_typed_queue *rtype, uint64_t tsc, int flag) {
    assert(sandbox != NULL);

    uint32_t head = rtype->rqueue_head;

    if (unlikely(head - rtype->rqueue_tail == RQUEUE_LEN)) {
        panic("Dispatcher dropped request as type %hhu because queue is full\n", rtype->type);
        return -1;
    } else {
        rtype->tsqueue[head & (RQUEUE_LEN - 1)] = tsc;
        rtype->rqueue[head & (RQUEUE_LEN - 1)] = sandbox;
	rtype->rqueue_head++;
        return 0;
    }
}

