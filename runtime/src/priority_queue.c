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

static inline void
priority_queue_update_highest_priority(struct priority_queue *self, const uint64_t priority)
{
	self->highest_priority = priority;
}

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
	assert(new_item != NULL);
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));

	int rc;

	if (unlikely(self->size + 1 > self->capacity)) panic("PQ overflow");
	if (self->size + 1 == self->capacity) goto err_enospc;
	self->items[++self->size] = new_item;

	rc = 0;
done:
	return rc;
err_enospc:
	rc = -ENOSPC;
	goto done;
}

/**
 * Checks if a priority queue is empty
 * @param self the priority queue to check
 * @returns true if empty, else otherwise
 */
static inline bool
priority_queue_is_empty(struct priority_queue *self)
{
	assert(self != NULL);
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));
	assert(!runtime_is_worker() || !software_interrupt_is_enabled());

	return self->size == 0;
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
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));

	/* If there's only one element, set memoized lookup and early out */
	if (self->size == 1) {
		priority_queue_update_highest_priority(self, self->get_priority_fn(self->items[1]));
		return;
	}

	for (int i = self->size;
	     i / 2 != 0 && self->get_priority_fn(self->items[i]) < self->get_priority_fn(self->items[i / 2]); i /= 2) {
		assert(self->get_priority_fn(self->items[i]) != ULONG_MAX);
		void *temp         = self->items[i / 2];
		self->items[i / 2] = self->items[i];
		self->items[i]     = temp;
		/* If percolated to highest priority, update highest priority */
		if (i / 2 == 1) priority_queue_update_highest_priority(self, self->get_priority_fn(self->items[1]));
	}
}

/**
 * Returns the index of a node's smallest child
 * @param self the priority queue
 * @param parent_index
 * @returns the index of the smallest child
 */
static inline int
priority_queue_find_smallest_child(struct priority_queue *self, const int parent_index)
{
	assert(self != NULL);
	assert(parent_index >= 1 && parent_index <= self->size);
	assert(self->get_priority_fn != NULL);
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));

	int left_child_index  = 2 * parent_index;
	int right_child_index = 2 * parent_index + 1;
	assert(self->items[left_child_index] != NULL);

	int smallest_child_idx;

	/* If we don't have a right child or the left child is smaller, return it */
	if (right_child_index > self->size) {
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
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));
	assert(runtime_is_worker());
	assert(!software_interrupt_is_enabled());

	bool update_highest_value = parent_index == 1;

	int left_child_index = 2 * parent_index;
	while (left_child_index >= 2 && left_child_index <= self->size) {
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

	/* Update memoized value if we touched the head */
	if (update_highest_value) {
		if (!priority_queue_is_empty(self)) {
			priority_queue_update_highest_priority(self, self->get_priority_fn(self->items[1]));
		} else {
			priority_queue_update_highest_priority(self, ULONG_MAX);
		}
	}
}

/*********************
 * Public API        *
 ********************/

/**
 * Initialized the Priority Queue Data structure
 * @param capacity the number of elements to store in the data structure
 * @param use_lock indicates that we want a concurrent data structure
 * @param get_priority_fn pointer to a function that returns the priority of an element
 * @return priority queue
 */
struct priority_queue *
priority_queue_initialize(size_t capacity, bool use_lock, priority_queue_get_priority_fn_t get_priority_fn)
{
	assert(get_priority_fn != NULL);
	assert(!runtime_is_worker() || !software_interrupt_is_enabled());

	/* Add one to capacity because this data structure ignores the element at 0 */
	size_t one_based_capacity = capacity + 1;

	struct priority_queue *self = calloc(sizeof(struct priority_queue) + sizeof(void *) * one_based_capacity, 1);


	/* We're assuming a min-heap implementation, so set to larget possible value */
	priority_queue_update_highest_priority(self, ULONG_MAX);
	self->size            = 0;
	self->capacity        = one_based_capacity; // Add one because we skip element 0
	self->get_priority_fn = get_priority_fn;
	self->use_lock        = use_lock;

	if (use_lock) LOCK_INIT(&self->lock);

	return self;
}

/**
 * Free the Priority Queue Data structure
 * @param self the priority_queue to initialize
 */
void
priority_queue_free(struct priority_queue *self)
{
	assert(self != NULL);
	assert(!runtime_is_worker() || !software_interrupt_is_enabled());

	free(self);
}

/**
 * @param self the priority_queue
 * @returns the number of elements in the priority queue
 */
int
priority_queue_length_nolock(struct priority_queue *self)
{
	assert(self != NULL);
	assert(runtime_is_worker());
	assert(!software_interrupt_is_enabled());
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));

	return self->size;
}

/**
 * @param self the priority_queue
 * @returns the number of elements in the priority queue
 */
int
priority_queue_length(struct priority_queue *self)
{
	LOCK_LOCK(&self->lock);
	int size = priority_queue_length_nolock(self);
	LOCK_UNLOCK(&self->lock);
	return size;
}

/**
 * @param self - the priority queue we want to add to
 * @param value - the value we want to add
 * @returns 0 on success. -ENOSPC on full.
 */
int
priority_queue_enqueue_nolock(struct priority_queue *self, void *value)
{
	assert(self != NULL);
	assert(value != NULL);
	assert(!runtime_is_worker() || !software_interrupt_is_enabled());
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));

	int rc;

	if (priority_queue_append(self, value) == -ENOSPC) goto err_enospc;

	priority_queue_percolate_up(self);

	rc = 0;
done:
	return rc;
err_enospc:
	rc = -ENOSPC;
	goto done;
}

/**
 * @param self - the priority queue we want to add to
 * @param value - the value we want to add
 * @returns 0 on success. -ENOSPC on full.
 */
int
priority_queue_enqueue(struct priority_queue *self, void *value)
{
	int rc;

	LOCK_LOCK(&self->lock);
	rc = priority_queue_enqueue_nolock(self, value);
	LOCK_UNLOCK(&self->lock);

	return rc;
}

/**
 * @param self - the priority queue we want to delete from
 * @param value - the value we want to delete
 * @returns 0 on success. -1 on not found
 */
int
priority_queue_delete_nolock(struct priority_queue *self, void *value)
{
	assert(self != NULL);
	assert(value != NULL);
	assert(runtime_is_worker());
	assert(!software_interrupt_is_enabled());
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));

	for (int i = 1; i <= self->size; i++) {
		if (self->items[i] == value) {
			self->items[i]            = self->items[self->size];
			self->items[self->size--] = NULL;
			priority_queue_percolate_down(self, i);
			return 0;
		}
	}

	return -1;
}

/**
 * @param self - the priority queue we want to delete from
 * @param value - the value we want to delete
 * @returns 0 on success. -1 on not found
 */
int
priority_queue_delete(struct priority_queue *self, void *value)
{
	int rc;

	LOCK_LOCK(&self->lock);
	rc = priority_queue_delete_nolock(self, value);
	LOCK_UNLOCK(&self->lock);

	return rc;
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
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty
 */
int
priority_queue_dequeue_nolock(struct priority_queue *self, void **dequeued_element)
{
	return priority_queue_dequeue_if_earlier_nolock(self, dequeued_element, UINT64_MAX);
}

/**
 * @param self - the priority queue we want to add to
 * @param dequeued_element a pointer to set to the dequeued element
 * @param target_deadline the deadline that the request must be earlier than in order to dequeue
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty or if none meet target_deadline
 */
int
priority_queue_dequeue_if_earlier_nolock(struct priority_queue *self, void **dequeued_element, uint64_t target_deadline)
{
	assert(self != NULL);
	assert(dequeued_element != NULL);
	assert(self->get_priority_fn != NULL);
	assert(runtime_is_worker());
	assert(!software_interrupt_is_enabled());
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));

	int return_code;

	/* If the dequeue is not higher priority (earlier timestamp) than targed_deadline, return immediately */
	if (priority_queue_is_empty(self) || self->highest_priority >= target_deadline) goto err_enoent;

	*dequeued_element         = self->items[1];
	self->items[1]            = self->items[self->size];
	self->items[self->size--] = NULL;

	priority_queue_percolate_down(self, 1);
	return_code = 0;

done:
	return return_code;
err_enoent:
	return_code = -ENOENT;
	goto done;
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
	int return_code;

	LOCK_LOCK(&self->lock);
	return_code = priority_queue_dequeue_if_earlier_nolock(self, dequeued_element, target_deadline);
	LOCK_UNLOCK(&self->lock);

	return return_code;
}

/**
 * Returns the top of the priority queue without removing it
 * @param self - the priority queue we want to add to
 * @param dequeued_element a pointer to set to the top element
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty
 */
int
priority_queue_top_nolock(struct priority_queue *self, void **dequeued_element)
{
	assert(self != NULL);
	assert(dequeued_element != NULL);
	assert(self->get_priority_fn != NULL);
	assert(runtime_is_worker());
	assert(!software_interrupt_is_enabled());
	assert(!self->use_lock || LOCK_IS_LOCKED(&self->lock));

	int return_code;

	if (priority_queue_is_empty(self)) goto err_enoent;

	*dequeued_element = self->items[1];
	return_code       = 0;

done:
	return return_code;
err_enoent:
	return_code = -ENOENT;
	goto done;
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
	int return_code;

	LOCK_LOCK(&self->lock);
	return_code = priority_queue_top_nolock(self, dequeued_element);
	LOCK_UNLOCK(&self->lock);

	return return_code;
}
