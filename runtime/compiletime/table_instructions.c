#include "types.h"
#include "wasm_store.h"

extern thread_local struct wasm_module_instance current_wasm_module_instance;

INLINE void
add_function_to_table(uint32_t idx, uint32_t type_id, char *pointer)
{
	wasm_table_set(current_wasm_module_instance.table, idx, type_id, pointer);
}

/* char * is used as a generic pointer to a function pointer */
INLINE char *
get_function_from_table(uint32_t idx, uint32_t type_id)
{
	return wasm_table_get(current_wasm_module_instance.table, idx, type_id);
}
