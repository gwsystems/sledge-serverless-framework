#pragma once

#include <stdint.h>

/* memory also provides the table access functions */
#define INDIRECT_TABLE_SIZE (1 << 10)

struct indirect_table_entry {
	uint32_t type_id;
	void *   func_pointer;
};
