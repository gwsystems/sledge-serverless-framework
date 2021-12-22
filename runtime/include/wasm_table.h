#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "sledge_abi.h"
#include "wasm_types.h"

/* Redeclared due to circular header dependency */
extern void sledge_abi__current_wasm_module_instance_trap(enum sledge_abi__wasm_trap trapno);


/* memory also provides the table access functions */
#define INDIRECT_TABLE_SIZE (1 << 10)

static INLINE int wasm_table_init(struct sledge_abi__wasm_table *wasm_table, size_t capacity);
static INLINE struct sledge_abi__wasm_table *wasm_table_alloc(size_t capacity);
static INLINE void                           wasm_table_deinit(struct sledge_abi__wasm_table *wasm_table);
static INLINE void                           wasm_table_free(struct sledge_abi__wasm_table *wasm_table);

static INLINE int
wasm_table_init(struct sledge_abi__wasm_table *wasm_table, size_t capacity)
{
	assert(wasm_table != NULL);

	if (capacity > 0) {
		wasm_table->buffer = calloc(capacity, sizeof(struct sledge_abi__wasm_table_entry));
		if (wasm_table->buffer == NULL) return -1;
	}

	wasm_table->capacity = capacity;
	wasm_table->length   = 0;

	return 0;
}

static INLINE struct sledge_abi__wasm_table *
wasm_table_alloc(size_t capacity)
{
	struct sledge_abi__wasm_table *wasm_table = (struct sledge_abi__wasm_table *)malloc(
	  sizeof(struct sledge_abi__wasm_table));
	if (wasm_table == NULL) return NULL;

	int rc = wasm_table_init(wasm_table, capacity);
	if (rc < 0) {
		wasm_table_free(wasm_table);
		return NULL;
	}

	return wasm_table;
}

static INLINE void
wasm_table_deinit(struct sledge_abi__wasm_table *wasm_table)
{
	assert(wasm_table != NULL);

	if (wasm_table->capacity > 0) {
		assert(wasm_table->buffer == NULL);
		assert(wasm_table->length == 0);
		return;
	}

	assert(wasm_table->buffer != NULL);
	free(wasm_table->buffer);
	wasm_table->buffer   = NULL;
	wasm_table->length   = 0;
	wasm_table->capacity = 0;
}

static INLINE void
wasm_table_free(struct sledge_abi__wasm_table *wasm_table)
{
	assert(wasm_table != NULL);
	free(wasm_table);
}

static INLINE void *
wasm_table_get(struct sledge_abi__wasm_table *wasm_table, uint32_t idx, uint32_t type_id)
{
	assert(wasm_table != NULL);

	if (unlikely(idx >= wasm_table->capacity)) {
		fprintf(stderr, "idx: %u, Table size: %u\n", idx, INDIRECT_TABLE_SIZE);
		sledge_abi__current_wasm_module_instance_trap(WASM_TRAP_INVALID_INDEX);
	}

	struct sledge_abi__wasm_table_entry f = wasm_table->buffer[idx];

	if (unlikely(f.type_id != type_id)) {
		fprintf(stderr, "Function Type mismatch. Expected: %u, Actual: %u\n", type_id, f.type_id);
		sledge_abi__current_wasm_module_instance_trap(WASM_TRAP_MISMATCHED_FUNCTION_TYPE);
	}

	if (unlikely(f.func_pointer == NULL)) {
		fprintf(stderr, "Function Type mismatch. Index %u resolved to NULL Pointer\n", idx);
		sledge_abi__current_wasm_module_instance_trap(WASM_TRAP_MISMATCHED_FUNCTION_TYPE);
	}

	assert(f.func_pointer != NULL);

	return f.func_pointer;
}

static INLINE void
wasm_table_set(struct sledge_abi__wasm_table *wasm_table, uint32_t idx, uint32_t type_id, char *pointer)
{
	assert(wasm_table != NULL);

	if (unlikely(idx >= wasm_table->capacity)) {
		fprintf(stderr, "idx: %u, Table size: %u\n", idx, INDIRECT_TABLE_SIZE);
		sledge_abi__current_wasm_module_instance_trap(WASM_TRAP_INVALID_INDEX);
	}

	assert(pointer != NULL);

	/* TODO: atomic for multiple concurrent invocations? Issue #97 */
	if (wasm_table->buffer[idx].type_id == type_id && wasm_table->buffer[idx].func_pointer == pointer) return;

	wasm_table->buffer[idx] = (struct sledge_abi__wasm_table_entry){ .type_id = type_id, .func_pointer = pointer };
}
