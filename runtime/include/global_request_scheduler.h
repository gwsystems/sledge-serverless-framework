#pragma once

#include <stdint.h>

#include "sandbox_request.h"

/* Returns pointer back if successful, null otherwise */
typedef struct sandbox_request *(*global_request_scheduler_add_fn_t)(struct sandbox_request *);
typedef int (*global_request_scheduler_remove_fn_t)(struct sandbox_request **);
typedef int (*global_request_scheduler_remove_if_earlier_fn_t)(struct sandbox_request **, uint64_t);
typedef int (*global_request_scheduler_remove_with_mt_class_fn_t)(struct sandbox_request **, uint64_t,
                                                                  enum MULTI_TENANCY_CLASS);
typedef uint64_t (*global_request_scheduler_peek_fn_t)(void);

struct global_request_scheduler_config {
	global_request_scheduler_add_fn_t                  add_fn;
	global_request_scheduler_remove_fn_t               remove_fn;
	global_request_scheduler_remove_if_earlier_fn_t    remove_if_earlier_fn;
	global_request_scheduler_remove_with_mt_class_fn_t remove_with_mt_class_fn;
	global_request_scheduler_peek_fn_t                 peek_fn;
};


void                    global_request_scheduler_initialize(struct global_request_scheduler_config *config);
struct sandbox_request *global_request_scheduler_add(struct sandbox_request *);
int                     global_request_scheduler_remove(struct sandbox_request **);
int                     global_request_scheduler_remove_if_earlier(struct sandbox_request **, uint64_t targed_deadline);
int      global_request_scheduler_remove_with_mt_class(struct sandbox_request **, uint64_t targed_deadline,
                                                       enum MULTI_TENANCY_CLASS mt_class);
uint64_t global_request_scheduler_peek(void);


static inline uint64_t
sandbox_request_get_priority_fn(void *element)
{
	struct sandbox_request *sandbox_request = (struct sandbox_request *)element;
	return sandbox_request->absolute_deadline;
};
