#pragma once

#include <stdbool.h>
#include <sandbox.h>

/* Returns pointer back if successful, null otherwise */
typedef struct sandbox *(*sandbox_run_queue_add_fn_t)(struct sandbox *);
typedef bool (*sandbox_run_queue_is_empty_fn_t)(void);
typedef void (*sandbox_run_queue_delete_fn_t)(struct sandbox *sandbox);
typedef struct sandbox *(*sandbox_run_queue_get_next_fn_t)();
typedef void (*sandbox_run_queue_preempt_fn_t)(ucontext_t *);

typedef struct sandbox_run_queue_config {
	sandbox_run_queue_add_fn_t      add;
	sandbox_run_queue_is_empty_fn_t is_empty;
	sandbox_run_queue_delete_fn_t delete;
	sandbox_run_queue_get_next_fn_t get_next;
	sandbox_run_queue_preempt_fn_t  preempt;
} sandbox_run_queue_config_t;


void sandbox_run_queue_initialize(sandbox_run_queue_config_t *config);


/* This is currently only used by worker_thread_wakeup_sandbox */
struct sandbox *sandbox_run_queue_add(struct sandbox *);
void            sandbox_run_queue_delete(struct sandbox *);
bool            sandbox_run_queue_is_empty();
struct sandbox *sandbox_run_queue_get_next();
void            sandbox_run_queue_preempt(ucontext_t *);
