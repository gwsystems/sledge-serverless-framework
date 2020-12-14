#include "admissions_control.h"

_Atomic uint64_t admissions_control_admitted;
uint64_t         admissions_control_capacity;

const double admissions_control_overhead = 0.2;
