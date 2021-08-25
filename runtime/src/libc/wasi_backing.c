#include "types.h"

extern __thread struct sandbox_context_cache local_sandbox_context_cache;
#define CURRENT_MEMORY_BASE  local_sandbox_context_cache.memory.start
#define CURRENT_MEMORY_SIZE  local_sandbox_context_cache.memory.size
#define CURRENT_WASI_CONTEXT local_sandbox_context_cache.wasi_context

#include "wasi_backing.h"
