#ifndef SFRT_SANDBOX_RUN_QUEUE_H
#define SFRT_SANDBOX_RUN_QUEUE_H

#include <stdbool.h>
#include <sandbox.h>

// Returns pointer back if successful, null otherwise
typedef struct sandbox *(*sandbox_run_queue_add_t)(struct sandbox *);
typedef bool (*sandbox_run_queue_is_empty_t)(void);
typedef void (*sandbox_run_queue_delete_t)(struct sandbox *sandbox);
typedef struct sandbox *(*sandbox_run_queue_get_next_t)();

typedef void (*sandbox_run_queue_preempt_t)(ucontext_t *);

typedef struct sandbox_run_queue_config_t {
	sandbox_run_queue_add_t      add;
	sandbox_run_queue_is_empty_t is_empty;
	sandbox_run_queue_delete_t delete;
	sandbox_run_queue_get_next_t get_next;
	sandbox_run_queue_preempt_t  preempt;
} sandbox_run_queue_config_t;


void sandbox_run_queue_initialize(sandbox_run_queue_config_t *config);


// This is currently only used by worker_thread_wakeup_sandbox
struct sandbox *sandbox_run_queue_add(struct sandbox *);
void            sandbox_run_queue_delete(struct sandbox *);
bool            sandbox_run_queue_is_empty();
struct sandbox *sandbox_run_queue_get_next();
void            sandbox_run_queue_preempt(ucontext_t *);


#endif /* SFRT_SANDBOX_RUN_QUEUE_H */