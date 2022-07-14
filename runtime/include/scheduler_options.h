#pragma once

enum SCHEDULER
{
	SCHEDULER_FIFO  = 0,
	SCHEDULER_EDF   = 1,
	SCHEDULER_MTDS  = 2,
	SCHEDULER_MTDBF = 3
};

extern enum SCHEDULER scheduler;
