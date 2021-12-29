#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <vec.h>

/* https://webassembly.github.io/spec/core/syntax/modules.html#globals */

enum wasm_global_type
{
	WASM_GLOBAL_TYPE_I32,
	WASM_GLOBAL_TYPE_I64,
	// WASM_GLOBAL_TYPE_F32,
	// WASM_GLOBAL_TYPE_F64,
	// WASM_GLOBAL_TYPE_V128,
	// WASM_GLOBAL_TYPE_FUNCREF,
	// WASM_GLOBAL_TYPE_EXTERNREF,
};


union wasm_global_value {
	int32_t i32;
	int64_t i64;
	// float f32;
	// double f64;
};

typedef struct wasm_global {
	enum wasm_global_type   type;
	bool                    mut;
	union wasm_global_value value;
} wasm_global_t;

VEC(wasm_global_t)

static inline int
wasm_globals_init(struct vec_wasm_global_t *globals, size_t capacity)
{
	return vec_wasm_global_t_init(globals, capacity);
}

static inline struct vec_wasm_global_t *
wasm_globals_alloc(size_t capacity)
{
	return vec_wasm_global_t_alloc(capacity);
}

static inline int32_t
wasm_globals_get_i32(struct vec_wasm_global_t *globals, size_t idx)
{
	wasm_global_t *global = vec_wasm_global_t_get(globals, idx);

	/* TODO: Replace with traps */
	if (global == NULL) assert(0);
	if (global->type != WASM_GLOBAL_TYPE_I32) assert(0);

	return global->value.i32;
}

static inline int32_t
wasm_globals_get_i64(struct vec_wasm_global_t *globals, size_t idx)
{
	wasm_global_t *global = vec_wasm_global_t_get(globals, idx);

	/* TODO: Replace with traps */
	if (global == NULL) assert(0);
	if (global->type != WASM_GLOBAL_TYPE_I64) assert(0);

	return global->value.i64;
}

/* TODO: Add API to set mutability */
static inline int32_t
wasm_globals_set_i32(struct vec_wasm_global_t *globals, size_t idx, int32_t value)
{
	/* TODO: Trap if immutable */
	int rc = vec_wasm_global_t_insert(globals, idx,
	                                  (wasm_global_t){ .mut = true, .type = WASM_GLOBAL_TYPE_I32, .value = value });
	return rc;
}

/* TODO: Add API to set mutability */
static inline int32_t
wasm_globals_set_i64(struct vec_wasm_global_t *globals, size_t idx, int64_t value)
{
	/* TODO: Trap if immutable */
	int rc = vec_wasm_global_t_insert(globals, idx,
	                                  (wasm_global_t){ .mut = true, .type = WASM_GLOBAL_TYPE_I64, .value = value });
	return rc;
}
