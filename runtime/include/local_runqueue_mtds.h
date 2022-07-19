#pragma once

#include "tenant.h"

void local_runqueue_mtds_initialize();
void local_runqueue_mtds_promote(struct perworker_tenant_sandbox_queue *);
void local_runqueue_mtds_demote(struct perworker_tenant_sandbox_queue *);
void local_timeout_queue_add(struct tenant *);
void local_timeout_queue_process_promotions();
