#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <spinlock/fas.h>

#define MAX 4096

/**
 * How to get the priority out of the generic element
 * We assume priority is expressed as an unsigned 64-bit integer (i.e. cycles or
 * UNIX time in ms). This is used to maintain a read replica of the highest
 * priority element that can be used to maintain a read replica
 * @param element
 * @returns priority (a u64)
 **/
typedef uint64_t (*priority_queue_get_priority_t)(void *element);

// We assume that priority is expressed in terms of a 64 bit unsigned integral
struct priority_queue {
	ck_spinlock_fas_t             lock;
	uint64_t                      highest_priority;
	void *                        items[MAX];
	int                           first_free;
	priority_queue_get_priority_t get_priority;
};

void     priority_queue_initialize(struct priority_queue *self, priority_queue_get_priority_t get_priority);
int      priority_queue_enqueue(struct priority_queue *self, void *value);
void *   priority_queue_dequeue(struct priority_queue *self);
int      priority_queue_length(struct priority_queue *self);
uint64_t priority_queue_peek(struct priority_queue *self);
int      priority_queue_delete(struct priority_queue *self, void *value);

#endif /* PRIORITY_QUEUE_H */