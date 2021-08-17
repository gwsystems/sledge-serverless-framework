#pragma once

#include <stdint.h>

/* FIXME: per-module configuration? Issue #101 */
#define WASM_PAGE_SIZE   (1024 * 64) /* 64KB */
#define WASM_START_PAGES (1 << 8)    /* 16MB */
#define WASM_MAX_PAGES   (1 << 15)   /* 4GB */
#define WASM_STACK_SIZE  (1 << 19)   /* 512KB */

/* bytes, not wasm pages */
struct wasm_memory {
	void *   start; /* after sandbox struct */
	uint32_t size;  /* from after sandbox struct */
	uint64_t max;   /* 4GB */
};
