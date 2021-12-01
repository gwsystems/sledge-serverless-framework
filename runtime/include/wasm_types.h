#pragma once

#include <stdint.h>

/* FIXME: per-module configuration? Issue #101 */
#define WASM_PAGE_SIZE            (1024 * 64) /* 64KB */
#define WASM_MEMORY_PAGES_INITIAL (1 << 8)    /* 256 Pages ~16MB */

#define WASM_STACK_SIZE (1 << 19) /* 512KB */
