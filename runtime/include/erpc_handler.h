#pragma once

#include <stdint.h>
void req_func(void *req_handle, uint8_t req_type, uint8_t *msg, size_t size, uint16_t port);
