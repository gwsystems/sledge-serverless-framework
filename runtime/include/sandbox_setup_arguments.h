#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sandbox_types.h"

/**
 * Takes the arguments from the sandbox struct and writes them into the WebAssembly linear memory
 */
static inline void
sandbox_setup_arguments(struct sandbox *sandbox)
{
	assert(sandbox != NULL);
	int32_t argument_count = 0;
	/* whatever gregor has, to be able to pass arguments to a module! */
	sandbox->arguments_offset = local_sandbox_context_cache.memory->size;
	assert(local_sandbox_context_cache.memory->data == sandbox->memory->data);
	expand_memory();

	int32_t string_off = sandbox->arguments_offset;

	stub_init(string_off);
}
