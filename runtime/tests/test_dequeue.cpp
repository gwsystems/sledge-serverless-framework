// C implementation of De-queue using circular array
// Maximum size of array or Dequeue
#include <stdio.h>
#include <assert.h>
#define MAX 100

using namespace std; 
// A structure to represent a Deque
struct Deque {
    int arr[MAX];
    int front;
    int rear;
    int size;
};


void init_deque(struct Deque * queue, int size) {
    assert(queue != NULL);
    queue->front = -1;
    queue->rear = 0;
    queue->size = size;
} 
 
// Checks whether Deque is full or not.
bool isFull(struct Deque * queue)
{
    assert(queue != NULL);
    return ((queue->front == 0 && queue->rear == queue->size - 1)
            || queue->front == queue->rear + 1);
}
 
// Checks whether Deque is empty or not.
bool isEmpty(struct Deque * queue) { 

    assert(queue != NULL);
    return (queue->front == -1); 
}
 
// Inserts an element at front
void insertfront(struct Deque * queue, int key)
{
    assert(queue != NULL);
    // check whether Deque if  full or not
    if (isFull(queue)) {
        printf( "Overflow\n");
        return;
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
 
    // insert current element into Deque
    queue->arr[queue->front] = key;
}
 
// function to inset element at rear end
// of Deque.
void insertrear(struct Deque * queue, int key)
{
    assert(queue != NULL);

    if (isFull(queue)) {
        printf(" Overflow\n");
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
 
    // insert current element into Deque
    queue->arr[queue->rear] = key;
}
 
// Deletes element at front end of Deque
void deletefront(struct Deque * queue)
{
    assert(queue != NULL);

    // check whether Deque is empty or not
    if (isEmpty(queue)) {
        printf("Queue Underflow\n");
        return;
    }
 
    // Deque has only one element
    if (queue->front == queue->rear) {
        queue->front = -1;
        queue->rear = -1;
    }
    else {
        // back to initial position
        if (queue->front == queue->size - 1)
            queue->front = 0;
 
   	else // increment front by '1' to remove current
            // front value from Deque
            queue->front = queue->front + 1;
    }
}
 
// Delete element at rear end of Deque
void deleterear(struct Deque * queue)
{
    assert(queue != NULL);
    if (isEmpty(queue)) {
        printf(" Underflow\n");
        return;
    }
 
    // Deque has only one element
    if (queue->front == queue->rear) {
        queue->front = -1;
        queue->rear = -1;
    }
    else if (queue->rear == 0)
        queue->rear = queue->size - 1;
    else
        queue->rear = queue->rear - 1;
}
 
// Returns front element of Deque
int getFront(struct Deque * queue)
{
    assert(queue != NULL);
    // check whether Deque is empty or not
    if (isEmpty(queue)) {
        printf(" Underflow\n");
        return -1;
    }
    return queue->arr[queue->front];
}
 
// function return rear element of Deque
int getRear(struct Deque * queue)
{
    assert(queue != NULL);
    // check whether Deque is empty or not
    if (isEmpty(queue) || queue->rear < 0) {
        printf(" Underflow\n");
        return -1;
    }
    return queue->arr[queue->rear];
}
 
// Driver code
int main()
{
    struct Deque dq;
    init_deque(&dq, 5);
   
    // Function calls
    printf("Insert element at rear end  : 5 \n");
    insertrear(&dq, 5);
 
    printf("insert element at rear end : 10 \n");
    insertrear(&dq, 10);
 
    printf("get rear element %d \n", getRear(&dq));
 
    deleterear(&dq);
    printf("After delete rear element new rear become %d ", getRear(&dq));
 
    printf("inserting element at front end \n");
    insertfront(&dq, 15);
 
    printf("get front element %d\n", getFront(&dq));
 
    deletefront(&dq);
 
    printf("After delete front element new front become %d", getFront(&dq));
    return 0;
}
