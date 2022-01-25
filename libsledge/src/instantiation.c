#include "sledge_abi.h"

#define EXPORT __attribute__((visibility("default")))
#define WEAK   __attribute__((weak))

/* aWsm ABI Symbols */
extern void     populate_globals(void);
extern void     populate_memory(void);
extern void     populate_table(void);
extern void     populate_table(void);
extern int32_t  wasmf__start(void);
extern uint32_t starting_pages;
extern uint32_t max_pages;

WEAK void populate_globals(){};

EXPORT void
sledge_abi__init_globals(void)
{
	populate_globals();
}

void
sledge_abi__init_mem(void)
{
	populate_memory();
}

EXPORT void
sledge_abi__init_tbl(void)
{
	populate_table();
}

EXPORT int32_t
sledge_abi__entrypoint(void)
{
	return wasmf__start();
}

EXPORT uint32_t
sledge_abi__wasm_memory_starting_pages(void)
{
	return starting_pages;
}

EXPORT uint32_t
sledge_abi__wasm_memory_max_pages(void)
{
	return max_pages;
}
