#pragma once
#include <stdbool.h>
#include <stdint.h> 
#include <stddef.h>

#define RDEQUE_LEN 65535 

struct request_typed_deque {
    uint8_t type;
    uint64_t deadline;
    struct sandbox *rqueue[RDEQUE_LEN];
    int front;
    int rear;
    int size;
    int length;
    uint64_t tsqueue[RDEQUE_LEN];
};

struct request_typed_deque * request_typed_deque_init(uint8_t type, int size); 
bool isFull(struct request_typed_deque * queue);
bool isEmpty(struct request_typed_deque * queue); 
int getLength(struct request_typed_deque * queue);
void insertfront(struct request_typed_deque * queue, struct sandbox * sandbox, uint64_t ts);
void insertrear(struct request_typed_deque * queue, struct sandbox * sandbox, uint64_t ts);
void deletefront(struct request_typed_deque * queue);
void deleterear(struct request_typed_deque * queue);
struct sandbox * getFront(struct request_typed_deque * queue, uint64_t * ts);
struct sandbox * getRear(struct request_typed_deque * queue, uint64_t * ts);
