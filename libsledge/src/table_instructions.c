#include "sledge_abi.h"

#define INDIRECT_TABLE_SIZE (1 << 10)
#define INLINE              __attribute__((always_inline))

/* This is private and NOT in the sledge_abi.h header because the runtime uses an overlay struct that extends this
 * symbol with private members */
extern thread_local struct sledge_abi__wasm_module_instance sledge_abi__current_wasm_module_instance;

static INLINE void *
wasm_table_get(struct sledge_abi__wasm_table *wasm_table, uint32_t idx, uint32_t type_id)
{
	assert(wasm_table != NULL);

	if (unlikely(idx >= wasm_table->capacity)) { sledge_abi__wasm_trap_raise(WASM_TRAP_INVALID_INDEX); }

	struct sledge_abi__wasm_table_entry f = wasm_table->buffer[idx];

	if (unlikely(f.type_id != type_id)) { sledge_abi__wasm_trap_raise(WASM_TRAP_MISMATCHED_TYPE); }

	if (unlikely(f.func_pointer == NULL)) { sledge_abi__wasm_trap_raise(WASM_TRAP_MISMATCHED_TYPE); }

	return f.func_pointer;
}

static INLINE void
wasm_table_set(struct sledge_abi__wasm_table *wasm_table, uint32_t idx, uint32_t type_id, char *pointer)
{
	assert(wasm_table != NULL);
	assert(idx < wasm_table->capacity);
	assert(pointer != NULL);

	if (wasm_table->buffer[idx].type_id == type_id && wasm_table->buffer[idx].func_pointer == pointer) return;
	wasm_table->buffer[idx] = (struct sledge_abi__wasm_table_entry){ .type_id = type_id, .func_pointer = pointer };
}

INLINE void
add_function_to_table(uint32_t idx, uint32_t type_id, char *pointer)
{
	wasm_table_set(sledge_abi__current_wasm_module_instance.table, idx, type_id, pointer);
}

/* char * is used as a generic pointer to a function pointer */
INLINE char *
get_function_from_table(uint32_t idx, uint32_t type_id)
{
	assert(sledge_abi__current_wasm_module_instance.table != NULL);

	if (unlikely(idx >= sledge_abi__current_wasm_module_instance.table->capacity)) {
		sledge_abi__wasm_trap_raise(WASM_TRAP_INVALID_INDEX);
	}

	struct sledge_abi__wasm_table_entry f = sledge_abi__current_wasm_module_instance.table->buffer[idx];

	if (unlikely(f.type_id != type_id)) { sledge_abi__wasm_trap_raise(WASM_TRAP_MISMATCHED_TYPE); }

	if (unlikely(f.func_pointer == NULL)) { sledge_abi__wasm_trap_raise(WASM_TRAP_MISMATCHED_TYPE); }

	return f.func_pointer;
}
