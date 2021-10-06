#pragma once

#include <stdbool.h>

#include "sandbox_types.h"

/* Returns pointer back if successful, null otherwise */
typedef void (*local_runqueue_add_fn_t)(struct sandbox *);
typedef bool (*local_runqueue_is_empty_fn_t)(void);
typedef void (*local_runqueue_delete_fn_t)(struct sandbox *sandbox);
typedef struct sandbox *(*local_runqueue_get_next_fn_t)();

struct local_runqueue_config {
	local_runqueue_add_fn_t      add_fn;
	local_runqueue_is_empty_fn_t is_empty_fn;
	local_runqueue_delete_fn_t   delete_fn;
	local_runqueue_get_next_fn_t get_next_fn;
};

void            local_runqueue_add(struct sandbox *);
void            local_runqueue_delete(struct sandbox *);
bool            local_runqueue_is_empty();
struct sandbox *local_runqueue_get_next();
void            local_runqueue_initialize(struct local_runqueue_config *config);
void 		local_workload_add();
void 		local_workload_complete();
