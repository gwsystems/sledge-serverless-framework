#include "scheduler.h"

enum SCHEDULER scheduler = SCHEDULER_EDF;
_Atomic uint32_t scheduling_counter = 0;
