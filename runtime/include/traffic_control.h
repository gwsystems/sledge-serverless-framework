#pragma once

#include <stdbool.h>
#include <stdint.h>

#define TRAFFIC_CONTROL
// #define LOG_TRAFFIC_CONTROL

typedef struct tenant tenant; // TODO: Why get circular dependency here?
typedef struct sandbox_metadata sandbox_metadata;
typedef enum dbf_update_mode dbf_update_mode_t;

void traffic_control_initialize(void);
void traffic_control_log_decision(const int admissions_case_num, const bool admitted);
uint64_t  traffic_control_decide(struct sandbox_metadata *sandbox_meta, uint64_t start_time, uint64_t estimated_execution, int *denial_code, int *worker_id_v);
uint64_t traffic_control_shed_work(struct tenant *tenant_to_exclude, uint64_t time_of_oversupply, int *worker_id_virt_just_shed, bool weak_shed);
