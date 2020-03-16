#ifndef SFRT_UTIL_H
#define SFRT_UTIL_H

#include <sandbox.h>
#include <module.h>


/**
 * Get CPU time in cycles using the Intel instruction rdtsc
 * @return CPU time in cycles
 **/
static unsigned long long int
rdtsc(void)
{
	unsigned long long int cpu_time_in_cycles = 0;
	unsigned int           cycles_lo;
	unsigned int           cycles_hi;
	__asm__ volatile("RDTSC" : "=a"(cycles_lo), "=d"(cycles_hi));
	cpu_time_in_cycles = (unsigned long long int)cycles_hi << 32 | cycles_lo;

	return cpu_time_in_cycles;
}

/* perhaps move it to module.h or sandbox.h? */
int util__parse_modules_file_json(char *filename);

#endif /* SFRT_UTIL_H */
