#pragma once

#include "tenant.h"

void local_runqueue_mtds_initialize();
void local_runqueue_mtds_promote(struct tenant_wrapper *);
void local_runqueue_mtds_demote(struct tenant_wrapper *);
void local_timeout_queue_add(struct tenant *);
void local_timeout_queue_process_promotions();
bool worker_tw_is_in_guaranteed_queue(struct tenant_wrapper *worker_tw);