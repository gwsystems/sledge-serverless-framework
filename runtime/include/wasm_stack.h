#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "sandbox_types.h"
#include "types.h"

struct wasm_stack {
	struct ps_list list;     /* Linked List Node used for object pool */
	size_t         capacity; /* Usable capacity. Excludes size of guard page that we need to free */
	uint8_t *      high;     /* The highest address of the stack. Grows down from here */
	uint8_t *      low;      /* The address of the lowest usabe address. Above guard page */
	uint8_t *      buffer;   /* Points to Guard Page */
};

static inline struct wasm_stack *
wasm_stack_allocate(void)
{
	return calloc(1, sizeof(struct wasm_stack));
}

/**
 * Allocates a static sized stack for a sandbox with a guard page underneath
 * Because a stack grows down, this protects against stack overflow
 * TODO: Should this use MAP_GROWSDOWN to enable demand paging for the stack?
 * @param sandbox sandbox that we want to allocate a stack for
 * @returns 0 on success, -1 on error
 */
static inline int
wasm_stack_init(struct wasm_stack *self, size_t capacity)
{
	assert(self);

	int rc = 0;

	self->buffer = (uint8_t *)mmap(NULL, /* guard page */ PAGE_SIZE + capacity, PROT_NONE,
	                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (unlikely(self->buffer == MAP_FAILED)) {
		perror("sandbox allocate stack");
		goto err_stack_allocation_failed;
	}

	self->low = (uint8_t *)mmap(self->buffer + /* guard page */ PAGE_SIZE, capacity, PROT_READ | PROT_WRITE,
	                            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (unlikely(self->low == MAP_FAILED)) {
		perror("sandbox set stack read/write");
		goto err_stack_prot_failed;
	}

	ps_list_init_d(self);
	self->capacity = capacity;
	self->high     = self->low + capacity;

	rc = 0;
done:
	return rc;
err_stack_prot_failed:
	rc = munmap(self->buffer, PAGE_SIZE + capacity);
	if (rc == -1) perror("munmap");
err_stack_allocation_failed:
	self->buffer = NULL;
	rc           = -1;
	goto done;
}

static INLINE void
wasm_stack_free(struct wasm_stack *self)
{
	free(self);
}


static struct wasm_stack *
wasm_stack_new(size_t capacity)
{
	struct wasm_stack *self = wasm_stack_allocate();
	int                rc   = wasm_stack_init(self, capacity);
	if (rc < 0) {
		wasm_stack_free(self);
		return NULL;
	}

	return self;
}

static inline void
wasm_stack_deinit(struct wasm_stack *self)
{
	assert(self != NULL);
	assert(self->buffer != NULL);

	/* The stack start is the bottom of the usable stack, but we allocated a guard page below this */
	munmap(self->buffer, self->capacity + PAGE_SIZE);
	self->buffer = NULL;
	self->high   = NULL;
	self->low    = NULL;
}

static inline void
wasm_stack_delete(struct wasm_stack *self)
{
	assert(self != NULL);
	assert(self->buffer != NULL);
	wasm_stack_deinit(self);
	wasm_stack_free(self);
}

static inline void
wasm_stack_reinit(struct wasm_stack *self)
{
	assert(self != NULL);
	assert(self->buffer != NULL);

	self->low = self->buffer + /* guard page */ PAGE_SIZE;

	memset(self->low, 0, self->capacity);
	ps_list_init_d(self);
	self->high = self->low + self->capacity;
}
