#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sandbox_types.h"

extern void stub_init(int32_t offset);

/**
 * Takes the arguments from the sandbox struct and writes them into the WebAssembly linear memory
 */
static inline void
sandbox_setup_arguments(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	int32_t argument_count = 0;

	/* Copy arguments into linear memory. It seems like malloc would clobber this, but I think this goes away in
	 * WASI, so not worth fixing*/

	sandbox->arguments_offset = wasm_memory_get_size(sandbox->memory);
	int rc                    = wasm_memory_expand(sandbox->memory, WASM_PAGE_SIZE);
	assert(rc == 0);

	stub_init(sandbox->arguments_offset);
}
