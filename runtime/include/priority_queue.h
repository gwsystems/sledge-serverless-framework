#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <errno.h>

#include "lock.h"
#include "listener_thread.h"
#include "panic.h"
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
	void                            *items[];
};

/**
 * Peek at the priority of the highest priority task without having to take the lock
 * Because this is a min-heap PQ, the highest priority is the lowest 64-bit integer
 * This is used to store an absolute deadline
 * @returns value of highest priority value in queue or ULONG_MAX if empty
 */
static inline uint64_t
priority_queue_peek(struct priority_queue *priority_queue)
{
	return priority_queue->highest_priority;
}


static inline void
priority_queue_update_highest_priority(struct priority_queue *priority_queue, const uint64_t priority)
{
	priority_queue->highest_priority = priority;
}

/**
 * Adds a value to the end of the binary heap
 * @param priority_queue the priority queue
 * @param new_item the value we are adding
 * @return 0 on success. -ENOSPC when priority queue is full
 */
static inline int
priority_queue_append(struct priority_queue *priority_queue, void *new_item)
{
	assert(priority_queue != NULL);
	assert(new_item != NULL);
	assert(!priority_queue->use_lock || lock_is_locked(&priority_queue->lock));

	int rc;

	if (unlikely(priority_queue->size > priority_queue->capacity)) panic("PQ overflow");
	if (unlikely(priority_queue->size == priority_queue->capacity)) goto err_enospc;
	priority_queue->items[++priority_queue->size] = new_item;

	rc = 0;
done:
	return rc;
err_enospc:
	rc = -ENOSPC;
	goto done;
}

/**
 * Checks if a priority queue is empty
 * @param priority_queue the priority queue to check
 * @returns true if empty, else otherwise
 */
static inline bool
priority_queue_is_empty(struct priority_queue *priority_queue)
{
	assert(priority_queue != NULL);
	assert(!priority_queue->use_lock || lock_is_locked(&priority_queue->lock));

	return priority_queue->size == 0;
}

/**
 * Shifts an appended value upwards to restore heap structure property
 * @param priority_queue the priority queue
 */
static inline void
priority_queue_percolate_up(struct priority_queue *priority_queue)
{
	assert(priority_queue != NULL);
	assert(priority_queue->get_priority_fn != NULL);
	assert(!priority_queue->use_lock || lock_is_locked(&priority_queue->lock));

	/* If there's only one element, set memoized lookup and early out */
	if (priority_queue->size == 1) {
		priority_queue_update_highest_priority(priority_queue,
		                                       priority_queue->get_priority_fn(priority_queue->items[1]));
		return;
	}

	for (int i = priority_queue->size; i / 2 != 0
	                                   && priority_queue->get_priority_fn(priority_queue->items[i])
	                                        < priority_queue->get_priority_fn(priority_queue->items[i / 2]);
	     i /= 2) {
		assert(priority_queue->get_priority_fn(priority_queue->items[i]) != ULONG_MAX);
		void *temp                   = priority_queue->items[i / 2];
		priority_queue->items[i / 2] = priority_queue->items[i];
		priority_queue->items[i]     = temp;
		/* If percolated to highest priority, update highest priority */
		if (i / 2 == 1)
			priority_queue_update_highest_priority(priority_queue, priority_queue->get_priority_fn(
			                                                         priority_queue->items[1]));
	}
}

/**
 * Returns the index of a node's smallest child
 * @param priority_queue the priority queue
 * @param parent_index
 * @returns the index of the smallest child
 */
static inline int
priority_queue_find_smallest_child(struct priority_queue *priority_queue, const int parent_index)
{
	assert(priority_queue != NULL);
	assert(parent_index >= 1 && parent_index <= priority_queue->size);
	assert(priority_queue->get_priority_fn != NULL);
	assert(!priority_queue->use_lock || lock_is_locked(&priority_queue->lock));

	int left_child_index  = 2 * parent_index;
	int right_child_index = 2 * parent_index + 1;
	assert(priority_queue->items[left_child_index] != NULL);

	int smallest_child_idx;

	/* If we don't have a right child or the left child is smaller, return it */
	if (right_child_index > priority_queue->size) {
		smallest_child_idx = left_child_index;
	} else if (priority_queue->get_priority_fn(priority_queue->items[left_child_index])
	           < priority_queue->get_priority_fn(priority_queue->items[right_child_index])) {
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
 * @param priority_queue the priority queue
 */
static inline void
priority_queue_percolate_down(struct priority_queue *priority_queue, int parent_index)
{
	assert(priority_queue != NULL);
	assert(priority_queue->get_priority_fn != NULL);
	assert(!priority_queue->use_lock || lock_is_locked(&priority_queue->lock));
	assert(!listener_thread_is_running());

	bool update_highest_value = parent_index == 1;

	int left_child_index = 2 * parent_index;
	while (left_child_index >= 2 && left_child_index <= priority_queue->size) {
		int smallest_child_index = priority_queue_find_smallest_child(priority_queue, parent_index);
		/* Once the parent is equal to or less than its smallest child, break; */
		if (priority_queue->get_priority_fn(priority_queue->items[parent_index])
		    <= priority_queue->get_priority_fn(priority_queue->items[smallest_child_index]))
			break;
		/* Otherwise, swap and continue down the tree */
		void *temp                                  = priority_queue->items[smallest_child_index];
		priority_queue->items[smallest_child_index] = priority_queue->items[parent_index];
		priority_queue->items[parent_index]         = temp;

		parent_index     = smallest_child_index;
		left_child_index = 2 * parent_index;
	}

	/* Update memoized value if we touched the head */
	if (update_highest_value) {
		if (!priority_queue_is_empty(priority_queue)) {
			priority_queue_update_highest_priority(priority_queue, priority_queue->get_priority_fn(
			                                                         priority_queue->items[1]));
		} else {
			priority_queue_update_highest_priority(priority_queue, ULONG_MAX);
		}
	}
}

/*********************
 * Public API        *
 ********************/

/**
 * @param priority_queue - the priority queue we want to add to
 * @param dequeued_element a pointer to set to the dequeued element
 * @param target_deadline the deadline that the request must be earlier than in order to dequeue
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty or if none meet target_deadline
 */
static inline int
priority_queue_dequeue_if_earlier_nolock(struct priority_queue *priority_queue, void **dequeued_element,
                                         uint64_t target_deadline)
{
	assert(priority_queue != NULL);
	assert(dequeued_element != NULL);
	assert(priority_queue->get_priority_fn != NULL);
	assert(!listener_thread_is_running());
	assert(!priority_queue->use_lock || lock_is_locked(&priority_queue->lock));

	int return_code;

	/* If the dequeue is not higher priority (earlier timestamp) than targed_deadline, return immediately */
	if (priority_queue_is_empty(priority_queue) || priority_queue->highest_priority >= target_deadline)
		goto err_enoent;

	*dequeued_element                             = priority_queue->items[1];
	priority_queue->items[1]                      = priority_queue->items[priority_queue->size];
	priority_queue->items[priority_queue->size--] = NULL;

	priority_queue_percolate_down(priority_queue, 1);
	return_code = 0;

done:
	return return_code;
err_enoent:
	return_code = -ENOENT;
	goto done;
}

/**
 * @param priority_queue - the priority queue we want to add to
 * @param dequeued_element a pointer to set to the dequeued element
 * @param target_deadline the deadline that the request must be earlier than in order to dequeue
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty or if none meet target_deadline
 */
static inline int
priority_queue_dequeue_if_earlier(struct priority_queue *priority_queue, void **dequeued_element,
                                  uint64_t target_deadline)
{
	int return_code;

	lock_node_t node = {};
	lock_lock(&priority_queue->lock, &node);
	return_code = priority_queue_dequeue_if_earlier_nolock(priority_queue, dequeued_element, target_deadline);
	lock_unlock(&priority_queue->lock, &node);

	return return_code;
}

/**
 * Initialized the Priority Queue Data structure
 * @param capacity the number of elements to store in the data structure
 * @param use_lock indicates that we want a concurrent data structure
 * @param get_priority_fn pointer to a function that returns the priority of an element
 * @return priority queue
 */
static inline struct priority_queue *
priority_queue_initialize(size_t capacity, bool use_lock, priority_queue_get_priority_fn_t get_priority_fn)
{
	assert(get_priority_fn != NULL);

	/* Add one to capacity because this data structure ignores the element at 0 */
	struct priority_queue *priority_queue = (struct priority_queue *)calloc(1, sizeof(struct priority_queue)
	                                                                             + sizeof(void *) * (capacity + 1));

	/* We're assuming a min-heap implementation, so set to larget possible value */
	priority_queue_update_highest_priority(priority_queue, ULONG_MAX);
	priority_queue->size            = 0;
	priority_queue->capacity        = capacity;
	priority_queue->get_priority_fn = get_priority_fn;
	priority_queue->use_lock        = use_lock;

	if (use_lock) lock_init(&priority_queue->lock);

	return priority_queue;
}

/**
 * Double capacity of priority queue
 * Note: currently there is no equivalent call for PQs that are not thread-local and need to be locked because it is
 * unclear if the fact that the lock is a member in the struct that might be moved by realloc breaks the guarantees of
 * the lock.
 * @param priority_queue to resize
 * @returns pointer to PR or NULL if realloc fails. This may have been moved by realloc!
 */
static inline struct priority_queue *
priority_queue_grow_nolock(struct priority_queue *priority_queue)
{
	assert(priority_queue != NULL);

	if (unlikely(priority_queue->capacity == 0)) {
		priority_queue->capacity++;
		debuglog("Growing to 1\n");
	} else {
		priority_queue->capacity *= 2;
		debuglog("Growing to %zu\n", priority_queue->capacity);
	}

	/* capacity is padded by 1 because idx 0 is unused */
	return (struct priority_queue *)realloc(priority_queue, sizeof(struct priority_queue)
	                                                          + sizeof(void *) * (priority_queue->capacity + 1));
}

/**
 * Free the Priority Queue Data structure
 * @param priority_queue the priority_queue to initialize
 */
static inline void
priority_queue_free(struct priority_queue *priority_queue)
{
	assert(priority_queue != NULL);

	free(priority_queue);
}

/**
 * @param priority_queue the priority_queue
 * @returns the number of elements in the priority queue
 */
static inline int
priority_queue_length_nolock(struct priority_queue *priority_queue)
{
	assert(priority_queue != NULL);
	assert(!priority_queue->use_lock || lock_is_locked(&priority_queue->lock));

	return priority_queue->size;
}

/**
 * @param priority_queue the priority_queue
 * @returns the number of elements in the priority queue
 */
static inline int
priority_queue_length(struct priority_queue *priority_queue)
{
	lock_node_t node = {};
	lock_lock(&priority_queue->lock, &node);
	int size = priority_queue_length_nolock(priority_queue);
	lock_unlock(&priority_queue->lock, &node);
	return size;
}

/**
 * @param priority_queue - the priority queue we want to add to
 * @param value - the value we want to add
 * @returns 0 on success. -ENOSPC on full.
 */
static inline int
priority_queue_enqueue_nolock(struct priority_queue *priority_queue, void *value)
{
	assert(priority_queue != NULL);
	assert(value != NULL);
	assert(!priority_queue->use_lock || lock_is_locked(&priority_queue->lock));

	int rc;

	if (unlikely(priority_queue_append(priority_queue, value) == -ENOSPC)) goto err_enospc;

	priority_queue_percolate_up(priority_queue);

	rc = 0;
done:
	return rc;
err_enospc:
	rc = -ENOSPC;
	goto done;
}

/**
 * @param priority_queue - the priority queue we want to add to
 * @param value - the value we want to add
 * @returns 0 on success. -ENOSPC on full.
 */
static inline int
priority_queue_enqueue(struct priority_queue *priority_queue, void *value)
{
	int rc;

	lock_node_t node = {};
	lock_lock(&priority_queue->lock, &node);
	rc = priority_queue_enqueue_nolock(priority_queue, value);
	lock_unlock(&priority_queue->lock, &node);

	return rc;
}

/**
 * @param priority_queue - the priority queue we want to delete from
 * @param value - the value we want to delete
 * @returns 0 on success. -1 on not found
 */
static inline int
priority_queue_delete_nolock(struct priority_queue *priority_queue, void *value)
{
	assert(priority_queue != NULL);
	assert(value != NULL);
	assert(!priority_queue->use_lock || lock_is_locked(&priority_queue->lock));

	for (int i = 1; i <= priority_queue->size; i++) {
		if (priority_queue->items[i] == value) {
			priority_queue->items[i]                      = priority_queue->items[priority_queue->size];
			priority_queue->items[priority_queue->size--] = NULL;
			priority_queue_percolate_down(priority_queue, i);
			return 0;
		}
	}

	return -1;
}

/**
 * @param priority_queue - the priority queue we want to delete from
 * @param value - the value we want to delete
 * @returns 0 on success. -1 on not found
 */
static inline int
priority_queue_delete(struct priority_queue *priority_queue, void *value)
{
	int rc;

	lock_node_t node = {};
	lock_lock(&priority_queue->lock, &node);
	rc = priority_queue_delete_nolock(priority_queue, value);
	lock_unlock(&priority_queue->lock, &node);

	return rc;
}

/**
 * @param priority_queue - the priority queue we want to add to
 * @param dequeued_element a pointer to set to the dequeued element
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty
 */
static inline int
priority_queue_dequeue(struct priority_queue *priority_queue, void **dequeued_element)
{
	return priority_queue_dequeue_if_earlier(priority_queue, dequeued_element, UINT64_MAX);
}

/**
 * @param priority_queue - the priority queue we want to add to
 * @param dequeued_element a pointer to set to the dequeued element
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty
 */
static inline int
priority_queue_dequeue_nolock(struct priority_queue *priority_queue, void **dequeued_element)
{
	return priority_queue_dequeue_if_earlier_nolock(priority_queue, dequeued_element, UINT64_MAX);
}

/**
 * Returns the top of the priority queue without removing it
 * @param priority_queue - the priority queue we want to add to
 * @param dequeued_element a pointer to set to the top element
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty
 */
static inline int
priority_queue_top_nolock(struct priority_queue *priority_queue, void **dequeued_element)
{
	assert(priority_queue != NULL);
	assert(dequeued_element != NULL);
	assert(priority_queue->get_priority_fn != NULL);
	assert(!priority_queue->use_lock || lock_is_locked(&priority_queue->lock));

	int return_code;

	if (priority_queue_is_empty(priority_queue)) goto err_enoent;

	*dequeued_element = priority_queue->items[1];
	return_code       = 0;

done:
	return return_code;
err_enoent:
	return_code = -ENOENT;
	goto done;
}

/**
 * Returns the top of the priority queue without removing it
 * @param priority_queue - the priority queue we want to add to
 * @param dequeued_element a pointer to set to the top element
 * @returns RC 0 if successfully set dequeued_element, -ENOENT if empty
 */
static inline int
priority_queue_top(struct priority_queue *priority_queue, void **dequeued_element)
{
	int return_code;

	lock_node_t node = {};
	lock_lock(&priority_queue->lock, &node);
	return_code = priority_queue_top_nolock(priority_queue, dequeued_element);
	lock_unlock(&priority_queue->lock, &node);

	return return_code;
}

#endif /* PRIORITY_QUEUE_H */
