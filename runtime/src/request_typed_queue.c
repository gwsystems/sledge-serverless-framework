#include <malloc.h>
#include <string.h>
#include <threads.h>

#include "panic.h"
#include "likely.h"
#include "request_typed_queue.h"

extern thread_local uint32_t current_reserved;
extern thread_local uint8_t dispatcher_thread_idx;

struct request_typed_queue *
request_typed_queue_init(uint8_t type, uint32_t n_resas) {
    struct request_typed_queue *queue = malloc(sizeof(struct request_typed_queue));
    queue->type_group = type;
    queue->rqueue_tail = 0;
    queue->rqueue_head = 0;
    queue->n_resas = n_resas;
    for (unsigned int i = 0; i < n_resas; ++i) {
        queue->res_workers[i] = current_reserved++;
    }

    for (unsigned int i = current_reserved; i < runtime_worker_group_size; i++) {
        queue->stealable_workers[queue->n_stealable++] = i;
    }

    if (queue->n_stealable == 0) {
        printf("Listener %u reserve %u cores (from %u to %u) to group id %u, can steal 0 core\n",
               dispatcher_thread_idx, n_resas, queue->res_workers[0], queue->res_workers[n_resas-1], type);
    } else {
        printf("Listener %u reserve %u cores (from %u to %u) to group id %u, can steal cores %u(from %u to %u)\n", 
            dispatcher_thread_idx, n_resas, queue->res_workers[0], queue->res_workers[n_resas-1], type, 
            queue->n_stealable, queue->stealable_workers[0], queue->stealable_workers[queue->n_stealable-1]);
    }
    memset(queue->rqueue, 0, RQUEUE_LEN * sizeof(struct sandbox*));

    return queue;

}

int push_to_rqueue(struct sandbox *sandbox, struct request_typed_queue *rtype, uint64_t tsc) {
    assert(sandbox != NULL);

    if (unlikely(rtype->rqueue_head - rtype->rqueue_tail == RQUEUE_LEN)) {
        panic("Dispatcher dropped request as group type %d because queue is full\n", rtype->type_group);
        return -1;
    } else {
        rtype->tsqueue[rtype->rqueue_head & (RQUEUE_LEN - 1)] = tsc;
        rtype->rqueue[rtype->rqueue_head++ & (RQUEUE_LEN - 1)] = sandbox;
        return 0;
    }
}

