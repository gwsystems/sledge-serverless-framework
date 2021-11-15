#pragma once

#include "global_request_scheduler.h"

void     global_request_scheduler_mts_initialize();
uint64_t global_request_scheduler_mts_guaranteed_peek();
uint64_t global_request_scheduler_mts_default_peek();
void     global_request_scheduler_mts_promote_lock(struct module_global_request_queue *);
void     global_request_scheduler_mts_demote_nolock(struct module_global_request_queue *);
void     global_timeout_queue_add(struct module *);
void     global_timeout_queue_check_for_promotions();
