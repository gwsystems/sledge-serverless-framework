#ifndef SFRT_RUNTIME_H
#define SFRT_RUNTIME_H
#include "types.h"

#include <uv.h>
#include "sandbox.h"

void alloc_linear_memory(void);
void expand_memory(void);
void free_linear_memory(void *base, u32 bound, u32 max);
// Assumption: bounds_check < WASM_PAGE_SIZE
INLINE char *get_memory_ptr_for_runtime(u32 offset, u32 bounds_check);

static inline void *
get_memory_ptr_void(u32 offset, u32 bounds_check)
{
	return (void*) get_memory_ptr_for_runtime(offset, bounds_check);
}

static inline char *
get_memory_string(u32 offset)
{
	char *naive_ptr = get_memory_ptr_for_runtime(offset, 1);
	int i = 0;

	while (1) {
		// Keep bounds checking the waters over and over until we know it's safe (we find a terminating character)
		char ith_element = get_memory_ptr_for_runtime(offset, i + 1)[i];

		if (ith_element == '\0') return naive_ptr;
		i++;
	}
}

INLINE char *get_function_from_table(u32 idx, u32 type_id);

// libc/* might need to do some setup for the libc setup
void stub_init(char *modulename, i32 offset, mod_init_libc_fn_t fn);

void runtime_init(void);

static inline void
runtime_on_alloc(uv_handle_t *h, size_t suggested, uv_buf_t *buf)
{
	buf->base = malloc(suggested);
	memset(buf->base, 0, suggested);
	buf->len = suggested;
}

extern __thread uv_loop_t uvio;
static inline uv_loop_t *
runtime_uvio(void)
{ return &uvio; }

#endif /* SFRT_RUNTIME_H */
