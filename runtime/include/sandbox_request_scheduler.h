#ifndef SFRT_SANDBOX_REQUEST_SCHEDULER_H
#define SFRT_SANDBOX_REQUEST_SCHEDULER_H

#include <sandbox_request.h>

// Returns pointer back if successful, null otherwise
typedef sandbox_request_t *(*sandbox_request_scheduler_add_t)(void *);
typedef sandbox_request_t *(*sandbox_request_scheduler_remove_t)(void);
typedef uint64_t (*sandbox_request_scheduler_peek_t)(void);

typedef struct sandbox_request_scheduler_config_t {
	sandbox_request_scheduler_add_t    add;
	sandbox_request_scheduler_remove_t remove;
	sandbox_request_scheduler_peek_t   peek;
} sandbox_request_scheduler_config_t;


void sandbox_request_scheduler_initialize(sandbox_request_scheduler_config_t *config);

sandbox_request_t *sandbox_request_scheduler_add(sandbox_request_t *);
sandbox_request_t *sandbox_request_scheduler_remove();
uint64_t           sandbox_request_scheduler_peek();

#endif /* SFRT_SANDBOX_REQUEST_QUEUE_H */