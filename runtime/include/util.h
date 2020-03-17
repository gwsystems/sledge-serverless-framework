#ifndef SFRT_UTIL_H
#define SFRT_UTIL_H

/**
 * Get CPU time in cycles using the Intel instruction rdtsc
 * @return CPU time in cycles
 **/
static unsigned long long int
util__rdtsc(void)
{
	unsigned long long int cpu_time_in_cycles = 0;
	unsigned int           cycles_lo;
	unsigned int           cycles_hi;
	__asm__ volatile("rdtsc" : "=a"(cycles_lo), "=d"(cycles_hi));
	cpu_time_in_cycles = (unsigned long long int)cycles_hi << 32 | cycles_lo;

	return cpu_time_in_cycles;
}

#endif /* SFRT_UTIL_H */
