#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "lock.h"
#include "runtime.h"
#include "worker_thread.h"

/**
 * How to get the priority out of the generic element
 * We assume priority is expressed as an unsigned 64-bit integer (i.e. cycles or
 * UNIX time in ms). This is used to maintain a read replica of the highest
 * priority element that can be used to maintain a read replica
 * @param element
 * @returns priority (a uint64_t)
 */
typedef uint64_t (*priority_queue_get_priority_fn_t)(void *element);

/* We assume that priority is expressed in terms of a 64 bit unsigned integral */
struct priority_queue {
	priority_queue_get_priority_fn_t get_priority_fn;
	bool                             use_lock;
	lock_t                           lock;
	uint64_t                         highest_priority;
	size_t                           size;
	size_t                           capacity;
	void *                           items[];
};

/**
 * Peek at the priority of the highest priority task without having to take the lock
 * Because this is a min-heap PQ, the highest priority is the lowest 64-bit integer
 * This is used to store an absolute deadline
 * @returns value of highest priority value in queue or ULONG_MAX if empty
 */
static inline uint64_t
priority_queue_peek(struct priority_queue *self)
{
	return self->highest_priority;
}


struct priority_queue *
     priority_queue_initialize(size_t capacity, bool use_lock, priority_queue_get_priority_fn_t get_priority_fn);
void priority_queue_free(struct priority_queue *self);

int priority_queue_length(struct priority_queue *self);
int priority_queue_length_nolock(struct priority_queue *self);

int priority_queue_enqueue(struct priority_queue *self, void *value);
int priority_queue_enqueue_nolock(struct priority_queue *self, void *value);

int priority_queue_delete(struct priority_queue *self, void *value);
int priority_queue_delete_nolock(struct priority_queue *self, void *value);

int priority_queue_dequeue(struct priority_queue *self, void **dequeued_element);
int priority_queue_dequeue_nolock(struct priority_queue *self, void **dequeued_element);

int priority_queue_dequeue_if_earlier(struct priority_queue *self, void **dequeued_element, uint64_t target_deadline);
int priority_queue_dequeue_if_earlier_nolock(struct priority_queue *self, void **dequeued_element,
                                             uint64_t target_deadline);

uint64_t priority_queue_peek(struct priority_queue *self);

int priority_queue_top(struct priority_queue *self, void **dequeued_element);
int priority_queue_top_nolock(struct priority_queue *self, void **dequeued_element);

#endif /* PRIORITY_QUEUE_H */
