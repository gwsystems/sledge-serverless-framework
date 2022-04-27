#pragma once

#include "module.h"

void local_runqueue_mtds_initialize();
void local_runqueue_mtds_promote(struct perworker_module_sandbox_queue *);
void local_runqueue_mtds_demote(struct perworker_module_sandbox_queue *);
void local_timeout_queue_add(struct module *);
void local_timeout_queue_process_promotions();
