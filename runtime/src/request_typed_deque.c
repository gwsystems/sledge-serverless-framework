#include <malloc.h>
#include <string.h>

#include "panic.h"
#include "likely.h"
#include "request_typed_deque.h"

struct request_typed_deque *
request_typed_deque_init(uint8_t type, int size) {
    struct request_typed_deque *deque = malloc(sizeof(struct request_typed_deque));
    assert(deque != NULL);
    assert(size <= RDEQUE_LEN);

    deque->type = type;
    deque->front = -1;
    deque->rear = 0;
    deque->size = size; 
    deque->length = 0;
    deque->deadline = 0;

    memset(deque->rqueue, 0, RDEQUE_LEN * sizeof(struct sandbox*));

    return deque;

}

// Checks whether request_typed_deque is full or not.
bool isFull(struct request_typed_deque * queue)
{
    assert(queue != NULL);
    return ((queue->front == 0 && queue->rear == queue->size - 1)
            || queue->front == queue->rear + 1);
}

// Checks whether request_typed_deque is empty or not.
bool isEmpty(struct request_typed_deque * queue) {

    assert(queue != NULL);
    return (queue->front == -1);
}

// Inserts an element at front
void insertfront(struct request_typed_deque * queue, struct sandbox * sandbox, uint64_t ts)
{
    assert(queue != NULL);
    // check whether request_typed_deque if  full or not
    if (isFull(queue)) {
        panic("Request typed deque overflow\n");
    }

    // If queue is initially empty
    if (queue->front == -1) {
        queue->front = 0;
        queue->rear = 0;
    }

    // front is at first position of queue
    else if (queue->front == 0)
        queue->front = queue->size - 1;

    else // decrement front end by '1'
        queue->front = queue->front - 1;

    // insert current element into request_typed_deque
    queue->rqueue[queue->front] = sandbox;
    queue->tsqueue[queue->front] = ts;
    queue->length++;
}

// function to inset element at rear end
// of request_typed_deque.
void insertrear(struct request_typed_deque * queue, struct sandbox * sandbox, uint64_t ts)
{
    assert(queue != NULL);

    if (isFull(queue)) {
        panic("Request typed deque overflow\n");
        return;
    }

    // If queue is initially empty
    if (queue->front == -1) {
        queue->front = 0;
        queue->rear = 0;
    }

    // rear is at last position of queue
    else if (queue->rear == queue->size - 1)
        queue->rear = 0;

    // increment rear end by '1'
    else
        queue->rear = queue->rear + 1;

    // insert current element into request_typed_deque
    queue->rqueue[queue->rear] = sandbox;
    queue->tsqueue[queue->rear] = ts;
    queue->length++;
}

// Deletes element at front end of request_typed_deque
void deletefront(struct request_typed_deque * queue)
{
    assert(queue != NULL);

    // check whether request_typed_deque is empty or not
    if (isEmpty(queue)) {
        printf("Queue Underflow\n");
        return;
    }
	
    /* reset the front item to NULL */
    queue->rqueue[queue->front] = NULL;

    // request_typed_deque has only one element
    if (queue->front == queue->rear) {
        queue->front = -1;
        queue->rear = -1;
    } else {
        // back to initial position
        if (queue->front == queue->size - 1) {
	    queue->front = 0;
	} else {// increment front by '1' to remove current
            // front value from request_typed_deque
            queue->front = queue->front + 1;
	}
    }
    queue->length--;
}


// Delete element at rear end of request_typed_deque
void deleterear(struct request_typed_deque * queue)
{
    assert(queue != NULL);
    if (isEmpty(queue)) {
        printf(" Underflow\n");
        return;
    }

    /* reset the front item to NULL */
    queue->rqueue[queue->rear] = NULL;

    // request_typed_deque has only one element
    if (queue->front == queue->rear) {
        queue->front = -1;
        queue->rear = -1;
    }
    else if (queue->rear == 0)
        queue->rear = queue->size - 1;
    else
        queue->rear = queue->rear - 1;
    queue->length--;
}

int getLength(struct request_typed_deque * queue) {
    assert(queue != NULL);
    return queue->length;
}

// Returns front element of request_typed_deque
struct sandbox * getFront(struct request_typed_deque * queue, uint64_t * ts)
{
    assert(queue != NULL);
    *ts = 0;
    // check whether request_typed_deque is empty or not
    if (isEmpty(queue)) {
        printf(" Underflow\n");
        return NULL;
    }
    *ts = queue->tsqueue[queue->front];
    return queue->rqueue[queue->front];
}

// function return rear element of request_typed_deque
struct sandbox * getRear(struct request_typed_deque * queue, uint64_t * ts)
{
    assert(queue != NULL);
    *ts = 0;
    // check whether request_typed_deque is empty or not
    if (isEmpty(queue) || queue->rear < 0) {
        printf(" Underflow\n");
        return NULL;
    }
    *ts = queue->tsqueue[queue->rear];
    return queue->rqueue[queue->rear];
}

