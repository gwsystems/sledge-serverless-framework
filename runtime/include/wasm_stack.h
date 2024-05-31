#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>

#include "ps_list.h"
#include "types.h"

/**
 * @brief wasm_stack is a stack used to execute an AOT-compiled WebAssembly instance. It is allocated with a static size
 * and a guard page beneath the lowest usuable address. Because the stack grows down, this protects against stack
 * overflow.
 *
 * Low Address  <---------------------------------------------------------------------------> High Address
 *              | GUARD PAGE    | USEABE FOR STACK FRAMES (SIZE of capacity)                |
 *             /\              /\                                                          /\
 *            buffer           low                                                        high
 *
 *                                                            | Frame 2 | Frame 1 | Frame 0 |
 *                                                          <<<<<<< Direction of Stack Growth
 */
struct wasm_stack {
	struct ps_list list;     /* Linked List Node used for object pool */
	uint64_t       capacity; /* Usable capacity. Excludes size of guard page that we need to free */
	uint8_t       *high;     /* The highest address of the stack. Grows down from here */
	uint8_t       *low;      /* The address of the lowest useabe address. Above guard page */
	uint8_t       *buffer;   /* Points base address of backing heap allocation (Guard Page) */
};

static struct wasm_stack *wasm_stack_alloc(uint64_t capacity);
static inline int         wasm_stack_init(struct wasm_stack *wasm_stack, uint64_t capacity);
static inline void        wasm_stack_reinit(struct wasm_stack *wasm_stack);
static inline void        wasm_stack_deinit(struct wasm_stack *wasm_stack);
static inline void        wasm_stack_free(struct wasm_stack *wasm_stack);

/**
 * Allocates a static sized stack for a sandbox with a guard page underneath
 * Because a stack grows down, this protects against stack overflow
 * TODO: Should this use MAP_GROWSDOWN to enable demand paging for the stack?
 * @param sandbox sandbox that we want to allocate a stack for
 * @returns 0 on success, -1 on error
 */
static inline int
wasm_stack_init(struct wasm_stack *wasm_stack, uint64_t capacity)
{
	assert(wasm_stack);

	int rc = 0;

	wasm_stack->buffer = (uint8_t *)mmap(NULL, /* guard page */ PAGE_SIZE + capacity, PROT_NONE,
	                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (unlikely(wasm_stack->buffer == MAP_FAILED)) {
		perror("sandbox allocate stack");
		goto err_stack_allocation_failed;
	}

	wasm_stack->low      = wasm_stack->buffer + /* guard page */ PAGE_SIZE;
	wasm_stack->capacity = capacity;
	wasm_stack->high     = wasm_stack->low + capacity;

	/* Set the initial bytes to read / write */
	rc = mprotect(wasm_stack->low, capacity, PROT_READ | PROT_WRITE);
	if (unlikely(rc != 0)) {
		perror("sandbox set stack read/write");
		goto err_stack_prot_failed;
	}

	ps_list_init_d(wasm_stack);

	rc = 0;
done:
	return rc;
err_stack_prot_failed:
	rc = munmap(wasm_stack->buffer, PAGE_SIZE + capacity);
	if (rc == -1) perror("munmap");
err_stack_allocation_failed:
	wasm_stack->buffer = NULL;
	rc                 = -1;
	goto done;
}

static struct wasm_stack *
wasm_stack_alloc(uint64_t capacity)
{
	struct wasm_stack *wasm_stack = calloc(1, sizeof(struct wasm_stack));
	int                rc         = wasm_stack_init(wasm_stack, capacity);
	if (rc < 0) {
		wasm_stack_free(wasm_stack);
		return NULL;
	}

	return wasm_stack;
}

static inline void
wasm_stack_deinit(struct wasm_stack *wasm_stack)
{
	assert(wasm_stack != NULL);
	assert(wasm_stack->buffer != NULL);

	/* The stack start is the bottom of the usable stack, but we allocated a guard page below this */
	munmap(wasm_stack->buffer, wasm_stack->capacity + PAGE_SIZE);
	wasm_stack->buffer = NULL;
	wasm_stack->high   = NULL;
	wasm_stack->low    = NULL;
}

static inline void
wasm_stack_free(struct wasm_stack *wasm_stack)
{
	assert(wasm_stack != NULL);
	assert(wasm_stack->buffer != NULL);
	wasm_stack_deinit(wasm_stack);
	free(wasm_stack);
}

static inline void
wasm_stack_reinit(struct wasm_stack *wasm_stack)
{
	assert(wasm_stack != NULL);
	assert(wasm_stack->buffer != NULL);
	assert(wasm_stack->low == wasm_stack->buffer + /* guard page */ PAGE_SIZE);
	assert(wasm_stack->high == wasm_stack->low + wasm_stack->capacity);

	explicit_bzero(wasm_stack->low, wasm_stack->capacity);
	ps_list_init_d(wasm_stack);
}
