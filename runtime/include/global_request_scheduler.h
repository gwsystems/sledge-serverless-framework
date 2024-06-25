#pragma once

#include <stdint.h>

#include "sandbox_types.h"
#include "sandbox_functions.h"

/* Returns pointer back if successful, null otherwise */
typedef struct sandbox *(*global_request_scheduler_add_fn_t)(struct sandbox *);
typedef int (*global_request_scheduler_remove_fn_t)(struct sandbox **);
typedef int (*global_request_scheduler_remove_if_earlier_fn_t)(struct sandbox **, uint64_t);
typedef uint64_t (*global_request_scheduler_peek_fn_t)(void);

struct global_request_scheduler_config {
	global_request_scheduler_add_fn_t               add_fn;
	global_request_scheduler_remove_fn_t            remove_fn;
	global_request_scheduler_remove_if_earlier_fn_t remove_if_earlier_fn;
	global_request_scheduler_peek_fn_t              peek_fn;
};


void                    global_request_scheduler_initialize(struct global_request_scheduler_config *config);
struct sandbox         *global_request_scheduler_add(struct sandbox *);
int                     global_request_scheduler_remove(struct sandbox **);
int                     global_request_scheduler_remove_if_earlier(struct sandbox **, uint64_t targed_deadline);
uint64_t                global_request_scheduler_peek(void);
void                    global_request_scheduler_update_highest_priority(const void *element);
struct sandbox_metadata global_request_scheduler_peek_metadata();
// struct sandbox_metadata global_request_scheduler_peek_metadata_must_lock(const uint64_t now, struct sandbox *);
void                    global_default_update_highest_priority(const void *element);
struct sandbox_metadata global_default_peek_metadata();
