#pragma once

#include <stdint.h>

/* bytes, not wasm pages */
struct wasm_memory {
	void *   start; /* after sandbox struct */
	uint32_t size;  /* from after sandbox struct */
	uint64_t max;   /* 4GB */
};
