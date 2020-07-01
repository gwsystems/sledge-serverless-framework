#pragma once

#include <sandbox_request.h>

/* Returns pointer back if successful, null otherwise */
typedef sandbox_request_t *(*global_request_scheduler_add_fn_t)(void *);
typedef sandbox_request_t *(*global_request_scheduler_remove_fn_t)(void);
typedef uint64_t (*global_request_scheduler_peek_fn_t)(void);

struct global_request_scheduler_config {
	global_request_scheduler_add_fn_t    add_fn;
	global_request_scheduler_remove_fn_t remove_fn;
	global_request_scheduler_peek_fn_t   peek_fn;
};


void global_request_scheduler_initialize(struct global_request_scheduler_config *config);

sandbox_request_t *global_request_scheduler_add(sandbox_request_t *);
sandbox_request_t *global_request_scheduler_remove();
uint64_t           global_request_scheduler_peek();
