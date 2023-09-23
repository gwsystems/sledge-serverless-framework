#pragma once

#include <stdint.h>
void edf_interrupt_req_handler(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
void darc_shinjuku_req_handler(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
