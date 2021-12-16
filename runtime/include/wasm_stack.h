#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "sandbox_types.h"
#include "types.h"

struct wasm_stack {
	size_t   capacity; /* Usable capacity. Excludes size of guard page that we need to free */
	uint8_t *high;     /* The highest address of the stack. Grows down from here */
	uint8_t *low;      /* The address of the lowest usabe address. Above guard page */
	uint8_t *buffer;   /* Points to Guard Page */
};

/**
 * Allocates a static sized stack for a sandbox with a guard page underneath
 * Because a stack grows down, this protects against stack overflow
 * TODO: Should this use MAP_GROWSDOWN to enable demand paging for the stack?
 * @param sandbox sandbox that we want to allocate a stack for
 * @returns 0 on success, -1 on error
 */
static INLINE int
wasm_stack_allocate(struct wasm_stack *stack, size_t capacity)
{
	assert(stack);

	int rc = 0;

	stack->buffer = (uint8_t *)mmap(NULL, /* guard page */ PAGE_SIZE + capacity, PROT_NONE,
	                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (unlikely(stack->buffer == MAP_FAILED)) {
		perror("sandbox allocate stack");
		goto err_stack_allocation_failed;
	}

	stack->low = (uint8_t *)mmap(stack->buffer + /* guard page */ PAGE_SIZE, capacity, PROT_READ | PROT_WRITE,
	                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (unlikely(stack->low == MAP_FAILED)) {
		perror("sandbox set stack read/write");
		goto err_stack_prot_failed;
	}

	stack->capacity = capacity;
	stack->high     = stack->low + capacity;

	rc = 0;
done:
	return rc;
err_stack_prot_failed:
	rc = munmap(stack->buffer, PAGE_SIZE + capacity);
	if (rc == -1) perror("munmap");
err_stack_allocation_failed:
	stack->buffer = NULL;
	rc            = -1;
	goto done;
}

static INLINE void
wasm_stack_free(struct wasm_stack *stack)
{
	assert(stack != NULL);
	assert(stack->buffer != NULL);
	/* The stack start is the bottom of the usable stack, but we allocated a guard page below this */
	int rc        = munmap(stack->buffer, stack->capacity + PAGE_SIZE);
	stack->buffer = NULL;
	if (unlikely(rc == -1)) perror("munmap");
}
