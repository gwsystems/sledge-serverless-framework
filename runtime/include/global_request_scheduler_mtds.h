#pragma once

#include "global_request_scheduler.h"

void     global_request_scheduler_mtds_initialize();
int      global_request_scheduler_mtds_remove_with_mt_class(struct sandbox **, uint64_t, enum MULTI_TENANCY_CLASS);
uint64_t global_request_scheduler_mtds_guaranteed_peek();
uint64_t global_request_scheduler_mtds_default_peek();
void     global_timeout_queue_add(struct tenant *);
void     global_request_scheduler_mtds_promote_lock(struct tenant_global_request_queue *);
void     global_request_scheduler_mtds_demote_nolock(struct tenant_global_request_queue *);
void     global_timeout_queue_process_promotions();
