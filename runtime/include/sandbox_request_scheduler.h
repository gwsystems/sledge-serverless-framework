#pragma once

#include <sandbox_request.h>

/* Returns pointer back if successful, null otherwise */
typedef sandbox_request_t *(*sandbox_request_scheduler_add_fn_t)(void *);
typedef sandbox_request_t *(*sandbox_request_scheduler_remove_fn_t)(void);
typedef uint64_t (*sandbox_request_scheduler_peek_fn_t)(void);

typedef struct sandbox_request_scheduler_config {
	sandbox_request_scheduler_add_fn_t    add;
	sandbox_request_scheduler_remove_fn_t remove;
	sandbox_request_scheduler_peek_fn_t   peek;
} sandbox_request_scheduler_config_t;


void sandbox_request_scheduler_initialize(sandbox_request_scheduler_config_t *config);

sandbox_request_t *sandbox_request_scheduler_add(sandbox_request_t *);
sandbox_request_t *sandbox_request_scheduler_remove();
uint64_t           sandbox_request_scheduler_peek();
