#pragma once

#include <stdint.h>
void edf_interrupt_req_handler(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
void darc_req_handler(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
void shinjuku_req_handler(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
void enqueue_to_global_queue_req_handler(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
void rr_req_handler(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
void rr_req_handler_without_interrupt(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
void jsq_req_handler(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
void jsq_req_handler_without_interrupt(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
void lld_req_handler(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
void lld_req_handler_without_interrupt(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
