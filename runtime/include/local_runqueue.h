#pragma once

#include <stdbool.h>

#include "sandbox_types.h"

/* Returns pointer back if successful, null otherwise */
typedef void (*local_runqueue_add_fn_t)(struct sandbox *);
typedef void (*local_runqueue_add_fn_t_idx)(int index, struct sandbox *);
typedef uint64_t (*local_runqueue_try_add_fn_t_idx)(int index, struct sandbox *, bool *need_interrupt);
typedef bool (*local_runqueue_is_empty_fn_t)(void);
typedef bool (*local_runqueue_is_empty_fn_t_idx)(int index);
typedef void (*local_runqueue_delete_fn_t)(struct sandbox *sandbox);
typedef struct sandbox *(*local_runqueue_get_next_fn_t)();
typedef int (*local_runqueue_get_height_fn_t)();
typedef int (*local_runqueue_get_length_fn_t)();
typedef int (*local_runqueue_get_length_fn_t_idx)(int index);
typedef void (*local_runqueue_print_in_order_fn_t_idx)(int index);

struct local_runqueue_config {
	local_runqueue_add_fn_t          add_fn;
	local_runqueue_add_fn_t_idx      add_fn_idx;
	local_runqueue_try_add_fn_t_idx  try_add_fn_idx;
	local_runqueue_is_empty_fn_t     is_empty_fn;
	local_runqueue_is_empty_fn_t_idx is_empty_fn_idx;
	local_runqueue_delete_fn_t       delete_fn;
	local_runqueue_get_next_fn_t     get_next_fn;
	local_runqueue_get_height_fn_t   get_height_fn;
	local_runqueue_get_length_fn_t   get_length_fn;
	local_runqueue_get_length_fn_t_idx   get_length_fn_idx;
	local_runqueue_print_in_order_fn_t_idx print_in_order_fn_idx;
};

void            local_runqueue_add(struct sandbox *);
void            local_runqueue_add_index(int index, struct sandbox *);
uint64_t        local_runqueue_try_add_index(int index, struct sandbox *, bool *need_interrupt);
void            local_runqueue_delete(struct sandbox *);
bool            local_runqueue_is_empty();
bool            local_runqueue_is_empty_index(int index);
struct sandbox *local_runqueue_get_next();
void            local_runqueue_initialize(struct local_runqueue_config *config);
int             local_runqueue_get_height();
int             local_runqueue_get_length();
int             local_runqueue_get_length_index(int index);
void		local_runqueue_print_in_order(int index);

void worker_queuing_cost_initialize();

void worker_queuing_cost_increment(int index, uint64_t cost);

void worker_queuing_cost_decrement(int index, uint64_t cost);

uint64_t get_local_queue_load(int index);

void wakeup_worker(int index);
