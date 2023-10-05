#include <assert.h>
#include <string.h>
#include <threads.h>

#include "panic.h"
#include "likely.h"
#include "request_fifo_queue.h"

extern thread_local int global_worker_thread_idx;
extern struct request_fifo_queue * worker_preempted_queue[1024];
thread_local static struct request_fifo_queue * local_preempted_queue = NULL;

/*
 * n_resas the number of reserved workers. Leaving the last n_resas workers as the reserved workers
 */
struct request_fifo_queue * request_fifo_queue_init() {
    struct request_fifo_queue * queue = (struct request_fifo_queue*) malloc(sizeof(struct request_fifo_queue));
   
    assert(queue != NULL);

    queue->rqueue_tail = 0;
    queue->rqueue_head = 0;
    memset(queue->rqueue, 0, RQUEUE_QUEUE_LEN * sizeof(struct sandbox*));
    return queue;
}

int push_to_queue(struct request_fifo_queue * queue, struct sandbox *sandbox, uint64_t tsc) {
    assert(sandbox != NULL);
    assert(queue != NULL);

    uint32_t head = queue->rqueue_head;

    if (unlikely(head - queue->rqueue_tail == RQUEUE_QUEUE_LEN)) {
        panic("request fifo queue is full\n");
        return -1;
    } else {
        queue->tsqueue[head & (RQUEUE_QUEUE_LEN - 1)] = tsc;
        queue->rqueue[head & (RQUEUE_QUEUE_LEN - 1)] = sandbox;
        queue->rqueue_head++;
        return 0;
    }

}

/* called by worker thread */
int push_to_preempted_queue(struct sandbox *sandbox, uint64_t tsc) {
    assert(sandbox != NULL);
    return push_to_queue(local_preempted_queue, sandbox, tsc);
}

/*
 * pop one item from the tail of the queue
 * tsc is the timestamp of the popped sandbox, only be set when popped sandbox is not NULL
 */
struct sandbox * pop_queue(struct request_fifo_queue * queue, uint64_t * tsc) {
    assert(queue != NULL);

    struct sandbox *popped_sandbox = NULL;
    *tsc = 0;

    if (queue->rqueue_head > queue->rqueue_tail) {
        popped_sandbox = queue->rqueue[queue->rqueue_tail & (RQUEUE_QUEUE_LEN - 1)];
        *tsc = queue->tsqueue[queue->rqueue_tail & (RQUEUE_QUEUE_LEN - 1)];
	queue->rqueue[queue->rqueue_tail & (RQUEUE_QUEUE_LEN - 1)] = NULL;
        queue->rqueue_tail++;
    }

    return popped_sandbox;
}

/* called by dispatcher thread */
struct sandbox * pop_worker_preempted_queue(int worker_id, struct request_fifo_queue * queue, uint64_t * tsc) {
    struct request_fifo_queue * preempted_queue = worker_preempted_queue[worker_id];
    assert(preempted_queue != NULL);

    return pop_queue(preempted_queue, tsc);
}

void local_preempted_fifo_queue_init() {
    local_preempted_queue = request_fifo_queue_init();
    worker_preempted_queue[global_worker_thread_idx] = local_preempted_queue;
}
