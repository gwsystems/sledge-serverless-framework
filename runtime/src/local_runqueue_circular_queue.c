#include <assert.h>
#include <string.h>
#include <threads.h>
#include <stdbool.h>

#include "local_runqueue.h"
#include "panic.h"
#include "likely.h"
#include "request_fifo_queue.h"

extern uint32_t local_queue_length[1024];
extern uint32_t max_local_queue_length[1024];
extern thread_local int global_worker_thread_idx;
extern struct request_fifo_queue * worker_circular_queue[1024];
thread_local static struct request_fifo_queue * local_runqueue_circular_queue = NULL;

/* Add item to the head of the queue */
void
local_runqueue_circular_queue_add(struct sandbox *sandbox) {
    assert(sandbox != NULL);
    assert(local_runqueue_circular_queue != NULL);
   
    uint32_t head = local_runqueue_circular_queue->rqueue_head;

    if (unlikely(head - local_runqueue_circular_queue->rqueue_tail == RQUEUE_QUEUE_LEN)) {
        panic("local circular runqueue is full\n");
    } else {
        local_runqueue_circular_queue->rqueue[head & (RQUEUE_QUEUE_LEN - 1)] = sandbox;
        local_runqueue_circular_queue->rqueue_head++;
    }

}

/* Called by diaptcher thread to add a new sandbox to the local runqueue */
void
local_runqueue_circular_queue_add_index(int index, struct sandbox *sandbox){
    assert(sandbox != NULL);

    struct request_fifo_queue * local_runqueue = worker_circular_queue[index];
    assert(local_runqueue != NULL);
    uint32_t head = local_runqueue->rqueue_head;

    if (unlikely(head - local_runqueue->rqueue_tail == RQUEUE_QUEUE_LEN)) {
        panic("local circular runqueue is full\n");
    } else {
        local_runqueue->rqueue[head & (RQUEUE_QUEUE_LEN - 1)] = sandbox;
        local_runqueue->rqueue_head++;
    }
    local_queue_length[index]++;
    if (local_queue_length[index] > max_local_queue_length[index]) {
	max_local_queue_length[index] = local_queue_length[index];
    }
}

bool
local_runqueue_circular_queue_is_empty() {
    assert(local_runqueue_circular_queue != NULL);
    return (local_runqueue_circular_queue->rqueue_head == local_runqueue_circular_queue->rqueue_tail);
}

/* Called by worker thread to delete item from the tail of the queue, the to be deleted sandbox must be in the tail */
void
local_runqueue_circular_queue_delete(struct sandbox *sandbox) {
    assert(sandbox != 0);
    assert(local_runqueue_circular_queue != NULL);
    assert(local_runqueue_circular_queue->rqueue[local_runqueue_circular_queue->rqueue_tail & (RQUEUE_QUEUE_LEN - 1)]
           == sandbox);
  
    local_runqueue_circular_queue->rqueue[local_runqueue_circular_queue->rqueue_tail & (RQUEUE_QUEUE_LEN - 1)]
           = NULL;

    local_runqueue_circular_queue->rqueue_tail++;
    local_queue_length[global_worker_thread_idx]--;
}

/* Called by worker thread to get item from the tail of the queue */
struct sandbox * local_runqueue_circular_queue_get_next() {
    assert(local_runqueue_circular_queue != NULL);
    if (local_runqueue_circular_queue->rqueue_head > local_runqueue_circular_queue->rqueue_tail) {
        struct sandbox *sandbox = 
        local_runqueue_circular_queue->rqueue[local_runqueue_circular_queue->rqueue_tail & (RQUEUE_QUEUE_LEN - 1)];
        assert(sandbox != NULL);
      	return sandbox;
    } else {
        return NULL;
    }
}

int
local_runqueue_circular_queue_get_length() {
    assert(local_runqueue_circular_queue != NULL);
    return (local_runqueue_circular_queue->rqueue_head - local_runqueue_circular_queue->rqueue_tail);
}

/**
 * Registers the PS variant with the polymorphic interface
 */
void
local_runqueue_circular_queue_initialize()
{
    /* Initialize local state */
    local_runqueue_circular_queue = (struct request_fifo_queue*) malloc(sizeof(struct request_fifo_queue));

    assert(local_runqueue_circular_queue != NULL);

    local_runqueue_circular_queue->rqueue_tail = 0;
    local_runqueue_circular_queue->rqueue_head = 0;
    memset(local_runqueue_circular_queue->rqueue, 0, RQUEUE_QUEUE_LEN * sizeof(struct sandbox*));

    worker_circular_queue[global_worker_thread_idx] = local_runqueue_circular_queue;


    /* Register Function Pointers for Abstract Scheduling API */
    struct local_runqueue_config config = { .add_fn         = local_runqueue_circular_queue_add,
                                            .add_fn_idx     = local_runqueue_circular_queue_add_index,
                                            .is_empty_fn    = local_runqueue_circular_queue_is_empty,
                                            .delete_fn      = local_runqueue_circular_queue_delete,
                                            .get_next_fn    = local_runqueue_circular_queue_get_next,
                                            .get_length_fn  = local_runqueue_circular_queue_get_length
                                          };

    local_runqueue_initialize(&config);
}


