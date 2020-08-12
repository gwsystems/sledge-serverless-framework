#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "panic.h"
#include "priority_queue.h"

/****************************
 * Private Helper Functions *
 ***************************/

/**
 * Adds a value to the end of the binary heap
 * @param self the priority queue
 * @param new_item the value we are adding
 * @return 0 on success. -ENOSPC when priority queue is full
 */
static inline int
priority_queue_append(struct priority_queue *self, void *new_item)
{
	assert(self != NULL);
	assert(LOCK_IS_LOCKED(&self->lock));

	int rc;

	if (self->first_free >= MAX) goto err_enospc;

	self->items[self->first_free++] = new_item;

	rc = 0;
done:
	return rc;
err_enospc:
	rc = -ENOSPC;
	goto done;
}

/**
 * Shifts an appended value upwards to restore heap structure property
 * @param self the priority queue
 */
static inline void
priority_queue_percolate_up(struct priority_queue *self)
{
	assert(self != NULL);
	assert(self->get_priority_fn != NULL);
	assert(LOCK_IS_LOCKED(&self->lock));

	for (int i = self->first_free - 1;
	     i / 2 != 0 && self->get_priority_fn(self->items[i]) < self->get_priority_fn(self->items[i / 2]); i /= 2) {
		assert(self->get_priority_fn(self->items[i]) != ULONG_MAX);
		void *temp         = self->items[i / 2];
		self->items[i / 2] = self->items[i];
		self->items[i]     = temp;
		/* If percolated to highest priority, update highest priority */
		if (i / 2 == 1) self->highest_priority = self->get_priority_fn(self->items[1]);
	}
}

/**
 * Returns the index of a node's smallest child
 * @param self the priority queue
 * @param parent_index
 * @returns the index of the smallest child
 */
static inline int
priority_queue_find_smallest_child(struct priority_queue *self, int parent_index)
{
	assert(self != NULL);
	assert(parent_index >= 1 && parent_index < self->first_free);
	assert(self->get_priority_fn != NULL);
	assert(LOCK_IS_LOCKED(&self->lock));

	int left_child_index  = 2 * parent_index;
	int right_child_index = 2 * parent_index + 1;
	assert(self->items[left_child_index] != NULL);

	int smallest_child_idx;

	/* If we don't have a right child or the left child is smaller, return it */
	if (right_child_index == self->first_free) {
		smallest_child_idx = left_child_index;
	} else if (self->get_priority_fn(self->items[left_child_index])
	           < self->get_priority_fn(self->items[right_child_index])) {
		smallest_child_idx = left_child_index;
	} else {
		/* Otherwise, return the right child */
		smallest_child_idx = right_child_index;
	}

	return smallest_child_idx;
}

/**
 * Shifts the top of the heap downwards. Used after placing the last value at
 * the top
 * @param self the priority queue
 */
static inline void
priority_queue_percolate_down(struct priority_queue *self, int parent_index)
{
	assert(self != NULL);
	assert(self->get_priority_fn != NULL);
	assert(LOCK_IS_LOCKED(&self->lock));

	int left_child_index = 2 * parent_index;
	while (left_child_index >= 2 && left_child_index < self->first_free) {
		int smallest_child_index = priority_queue_find_smallest_child(self, parent_index);
		/* Once the parent is equal to or less than its smallest child, break; */
		if (self->get_priority_fn(self->items[parent_index])
		    <= self->get_priority_fn(self->items[smallest_child_index]))
			break;
		/* Otherwise, swap and continue down the tree */
		void *temp                        = self->items[smallest_child_index];
		self->items[smallest_child_index] = self->items[parent_index];
		self->items[parent_index]         = temp;

		parent_index     = smallest_child_index;
		left_child_index = 2 * parent_index;
	}
}

/**
 * Checks if a priority queue is empty
 * @param self the priority queue to check
 * @returns true if empty, else otherwise
 */
static inline bool
priority_queue_is_empty_locked(struct priority_queue *self)
{
	assert(self != NULL);
	assert(LOCK_IS_LOCKED(&self->lock));
	return self->first_free == 1;
}

/*********************
 * Public API        *
 ********************/

/**
 * Initialized the Priority Queue Data structure
 * @param self the priority_queue to initialize
 * @param get_priority_fn pointer to a function that returns the priority of an element
 */
void
priority_queue_initialize(struct priority_queue *self, priority_queue_get_priority_fn_t get_priority_fn)
{
	assert(self != NULL);
	assert(get_priority_fn != NULL);

	memset(self->items, 0, sizeof(void *) * MAX);

	LOCK_INIT(&self->lock);
	self->first_free      = 1;
	self->get_priority_fn = get_priority_fn;

	/* We're assuming a min-heap implementation, so set to larget possible value */
	self->highest_priority = ULONG_MAX;
}

/**
 * @param self the priority_queue
 * @returns the number of elements in the priority queue
 */
int
priority_queue_length(struct priority_queue *self)
{
	assert(self != NULL);

	LOCK_LOCK(&self->lock);
	int length = self->first_free - 1;
	LOCK_UNLOCK(&self->lock);
	return length;
}

/**
 * @param self - the priority queue we want to add to
 * @param value - the value we want to add
 * @returns 0 on success. -ENOSPC on full.
 */
int
priority_queue_enqueue(struct priority_queue *self, void *value)
{
	assert(self != NULL);

	int rc;

	LOCK_LOCK(&self->lock);

	if (priority_queue_append(self, value) == -ENOSPC) goto err_enospc;

	/* If this is the first element we add, update the highest priority */
	if (self->first_free == 2) {
		self->highest_priority = self->get_priority_fn(value);
	} else {
		priority_queue_percolate_up(self);
	}

	rc = 0;
release_lock:
	LOCK_UNLOCK(&self->lock);
done:
	return rc;
err_enospc:
	rc = -ENOSPC;
	goto release_lock;
}

/**
 * @param self - the priority queue we want to delete from
 * @param value - the value we want to delete
 * @returns 0 on success. -1 on not found
 */
int
priority_queue_delete(struct priority_queue *self, void *value)
{
	assert(self != NULL);

	LOCK_LOCK(&self->lock);

	bool did_delete = false;
	for (int i = 1; i < self->first_free; i++) {
		if (self->items[i] == value) {
			self->items[i]                = self->items[--self->first_free];
			self->items[self->first_free] = NULL;
			priority_queue_percolate_down(self, i);
			did_delete = true;
		}
	}

	LOCK_UNLOCK(&self->lock);

	if (!did_delete) return -1;
	return 0;
}

/**
 * @param self - the priority queue we want to add to
 * @param dequeued_element a pointer to set to the dequeued element
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty
 */
int
priority_queue_dequeue(struct priority_queue *self, void **dequeued_element)
{
	return priority_queue_dequeue_if_earlier(self, dequeued_element, UINT64_MAX);
}

/**
 * @param self - the priority queue we want to add to
 * @param dequeued_element a pointer to set to the dequeued element
 * @param target_deadline the deadline that the request must be earlier than in order to dequeue
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty or if none meet target_deadline
 */
int
priority_queue_dequeue_if_earlier(struct priority_queue *self, void **dequeued_element, uint64_t target_deadline)
{
	assert(self != NULL);
	assert(dequeued_element != NULL);
	assert(self->get_priority_fn != NULL);

	int return_code;

	LOCK_LOCK(&self->lock);

	/* If the dequeue is not higher priority (earlier timestamp) than targed_deadline, return immediately */
	if (priority_queue_is_empty_locked(self) || self->highest_priority >= target_deadline) goto err_enoent;

	*dequeued_element             = self->items[1];
	self->items[1]                = self->items[--self->first_free];
	self->items[self->first_free] = NULL;
	/* Because of 1-based indices, first_free is 2 when there is only one element */
	if (self->first_free > 2) priority_queue_percolate_down(self, 1);

	/* Update the highest priority */
	if (!priority_queue_is_empty_locked(self)) {
		self->highest_priority = self->get_priority_fn(self->items[1]);
	} else {
		self->highest_priority = ULONG_MAX;
	}
	return_code = 0;

release_lock:
	LOCK_UNLOCK(&self->lock);
done:
	return return_code;
err_enoent:
	return_code = -ENOENT;
	goto release_lock;
}

/**
 * Returns the top of the priority queue without removing it
 * @param self - the priority queue we want to add to
 * @param dequeued_element a pointer to set to the top element
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty
 */
int
priority_queue_top(struct priority_queue *self, void **dequeued_element)
{
	assert(self != NULL);
	assert(dequeued_element != NULL);
	assert(self->get_priority_fn != NULL);

	int return_code;

	LOCK_LOCK(&self->lock);

	if (priority_queue_is_empty_locked(self)) goto err_enoent;

	*dequeued_element = self->items[1];
	return_code       = 0;

release_lock:
	LOCK_UNLOCK(&self->lock);
done:
	return return_code;
err_enoent:
	return_code = -ENOENT;
	goto release_lock;
}

/**
 * Peek at the priority of the highest priority task without having to take the lock
 * Because this is a min-heap PQ, the highest priority is the lowest 64-bit integer
 * This is used to store an absolute deadline
 * @returns value of highest priority value in queue or ULONG_MAX if empty
 */
uint64_t
priority_queue_peek(struct priority_queue *self)
{
	return self->highest_priority;
}
