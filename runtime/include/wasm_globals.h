#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <vec.h>

/* https://webassembly.github.io/spec/core/syntax/modules.html#globals */

/* This only supports i32 and i64 because this is all that aWsm currently supports */
enum wasm_global_type
{
	WASM_GLOBAL_TYPE_UNUSED,
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

static inline void
wasm_globals_deinit(struct vec_wasm_global_t *globals)
{
	vec_wasm_global_t_deinit(globals);
}

static inline int
wasm_globals_init(struct vec_wasm_global_t *globals, uint32_t capacity)
{
	return vec_wasm_global_t_init(globals, capacity);
}

static inline void
wasm_globals_update_if_used(struct vec_wasm_global_t *globals, uint32_t idx, uint64_t *dest)
{
	wasm_global_t *global = vec_wasm_global_t_get(globals, idx);
	if (likely(global->type != WASM_GLOBAL_TYPE_UNUSED)) *dest = (uint64_t)global->value.i64;
}

static inline int
wasm_globals_get_i32(struct vec_wasm_global_t *globals, uint32_t idx, int32_t *return_val)
{
	wasm_global_t *global = vec_wasm_global_t_get(globals, idx);

	if (unlikely(global == NULL)) return -1;
	if (unlikely(global->type != WASM_GLOBAL_TYPE_I32)) return -2;

	*return_val = global->value.i32;
	return 0;
}

static inline int
wasm_globals_get_i64(struct vec_wasm_global_t *globals, uint32_t idx, int64_t *return_val)
{
	wasm_global_t *global = vec_wasm_global_t_get(globals, idx);

	if (unlikely(global == NULL)) return -1;
	if (unlikely(global->type != WASM_GLOBAL_TYPE_I64)) return -2;

	*return_val = global->value.i64;
	return 0;
}

// 0 on success, -1 on out of bounds, -2 on mismatched type
static inline int
wasm_globals_set_i32(struct vec_wasm_global_t *globals, uint32_t idx, int32_t value, bool is_mutable)
{
	wasm_global_t *current = vec_wasm_global_t_get(globals, idx);
	if (unlikely(current->type != WASM_GLOBAL_TYPE_UNUSED && current->mut == false)) return -2;

	int rc = vec_wasm_global_t_insert(globals, idx,
	                                  (wasm_global_t){
	                                    .mut = is_mutable, .type = WASM_GLOBAL_TYPE_I32, .value = value });
	return rc;
}

// 0 on success, -1 on out of bounds, -2 on mismatched type
static inline int
wasm_globals_set_i64(struct vec_wasm_global_t *globals, uint32_t idx, int64_t value, bool is_mutable)
{
	wasm_global_t *current = vec_wasm_global_t_get(globals, idx);
	if (unlikely(current->type != WASM_GLOBAL_TYPE_UNUSED && current->mut == false)) return -2;

	// Returns -1 if idx > capacity
	int rc = vec_wasm_global_t_insert(globals, idx,
	                                  (wasm_global_t){
	                                    .mut = is_mutable, .type = WASM_GLOBAL_TYPE_I64, .value = value });
	return rc;
}
