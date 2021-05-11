#pragma once

#include <stdbool.h>
#include <stdint.h>

#define ADMISSIONS_CONTROL_GRANULARITY 1000000

void     admissions_control_initialize(void);
void     admissions_control_add(uint64_t admissions_estimate);
void     admissions_control_subtract(uint64_t admissions_estimate);
uint64_t admissions_control_calculate_estimate(uint64_t estimated_execution, uint64_t relative_deadline);
uint64_t admissions_control_calculate_estimate_us(uint32_t estimated_execution_us, uint32_t relative_deadline_us);
void     admissions_control_log_decision(uint64_t admissions_estimate, bool admitted);
uint64_t admissions_control_decide(uint64_t admissions_estimate);
