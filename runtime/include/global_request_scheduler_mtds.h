#pragma once

#include "global_request_scheduler.h"

void     global_request_scheduler_mtds_initialize();
uint64_t global_request_scheduler_mtds_guaranteed_peek();
uint64_t global_request_scheduler_mtds_default_peek();
void     global_timeout_queue_add(struct module *);
void     global_request_scheduler_mtds_promote_lock(struct module_global_request_queue *);
void     global_request_scheduler_mtds_demote_nolock(struct module_global_request_queue *);
void     global_timeout_queue_process_promotions();
