#pragma once

/* Register context saved and restored on user-level, direct context switches. */
typedef enum
{
	UREG_SP = 0,
	UREG_IP = 1,
	UREG_COUNT
} ureg_t;
