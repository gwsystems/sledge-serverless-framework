#pragma once
#include <stdint.h>
#include <stdio.h>

struct mem_logging_obj {
	uint32_t	log_size;
	char 		*logger;
	uint32_t	offset;
	FILE*		fout;
};

void mem_log_init(uint32_t log_size, char const* file);
void mem_log_init2(uint32_t log_size, FILE* file);
void mem_log(char const * fmt, ...);
void dump_log_to_file();
