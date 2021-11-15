#pragma once

void local_runqueue_mts_initialize();
void local_runqueue_mts_promote(struct perworker_module_sandbox_queue *);
void local_runqueue_mts_demote(struct perworker_module_sandbox_queue *);
void local_timeout_queue_add(struct module *);
void local_timeout_queue_check_for_promotions();
