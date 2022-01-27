#include "sledge_abi.h"

/* The visibility attribute is used to control the visibility of a symbol across C translation units. The default
 * argument forces "external" linkage. This originated in gcc, but had been adopted by LLVM.
 * Reference: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html */
#define EXPORT __attribute__((visibility("default")))

/* The weak attribute is used to provide a weak symbol that can be overridden by later linking to a "strong" symbol.
 * This is useful for defining a default or no-op implementation of a symbol that might or might not be present in a
 * binary. This applies to populate_globals() below, which is only generated by aWsm when a module is compiled with the
 * "--runtime-globals" argument. We need to provide a weak symbol that does nothing to prevent unresolved symbols from
 * triggering linker errors. */
#define WEAK __attribute__((weak))

/* aWsm ABI Symbols */
extern void     populate_globals(void);
extern void     populate_memory(void);
extern void     populate_table(void);
extern void     populate_table(void);
extern void     wasmf___init_libc(int32_t, int32_t);
extern int32_t  wasmf_main(int32_t, int32_t);
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

// Wasmception Initialization. Unsure what a and b is here
EXPORT void
sledge_abi__init_libc(int32_t envp, int32_t pn)
{
	wasmf___init_libc(envp, pn);
}

EXPORT int32_t
sledge_abi__entrypoint(int32_t argc, int32_t argv)
{
	return wasmf_main(argc, argv);
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
