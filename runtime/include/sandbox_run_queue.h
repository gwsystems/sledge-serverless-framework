#ifndef SFRT_SANDBOX_RUN_QUEUE_H
#define SFRT_SANDBOX_RUN_QUEUE_H

#include <stdbool.h>
#include <sandbox.h>

// Returns pointer back if successful, null otherwise
typedef struct sandbox *(*sandbox_run_queue_add_t)(struct sandbox *);
typedef struct sandbox *(*sandbox_run_queue_remove_t)(void);
typedef bool (*sandbox_run_queue_is_empty_t)(void);

typedef struct sandbox_run_queue_config_t {
	sandbox_run_queue_add_t      add;
	sandbox_run_queue_is_empty_t is_empty;
	sandbox_run_queue_remove_t   remove;
} sandbox_run_queue_config_t;


void sandbox_run_queue_initialize(sandbox_run_queue_config_t *config);

struct sandbox *sandbox_run_queue_add(struct sandbox *);
struct sandbox *sandbox_run_queue_remove();
bool            sandbox_run_queue_is_empty();

#endif /* SFRT_SANDBOX_RUN_QUEUE_H */