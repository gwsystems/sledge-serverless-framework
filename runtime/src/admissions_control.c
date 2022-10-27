#include <assert.h>
#include <stdatomic.h>
#include <unistd.h>

#include "admissions_control.h"
#include "debuglog.h"
#include "likely.h"
#include "panic.h"
#include "runtime.h"

/*
 * Unitless estimate of the instantaneous fraction of system capacity required to complete all previously
 * admitted work. This is used to calculate free capacity as part of admissions control
 *
 * The estimated requirements of a single admitted request is calculated as
 * estimated execution time (cycles) / relative deadline (cycles)
 *
 * These estimates are incremented on request acceptance and decremented on request completion (either
 * success or failure)
 */

#ifdef ADMISSIONS_CONTROL
_Atomic uint64_t admissions_control_admitted;
uint64_t         admissions_control_capacity;
const double     admissions_control_overhead = 0.2;
#endif

void
admissions_control_initialize()
{
#ifdef ADMISSIONS_CONTROL
	atomic_init(&admissions_control_admitted, 0);
	admissions_control_capacity = runtime_worker_threads_count * ADMISSIONS_CONTROL_GRANULARITY
	                              * ((double)1.0 - admissions_control_overhead);
#endif
}

void
admissions_control_add(uint64_t admissions_estimate)
{
#ifdef ADMISSIONS_CONTROL
	assert(admissions_estimate > 0);
	//atomic_fetch_add(&admissions_control_admitted, admissions_estimate);

#ifdef LOG_ADMISSIONS_CONTROL
	debuglog("Runtime Admitted: %lu / %lu\n", admissions_control_admitted, admissions_control_capacity);
#endif

#endif /* ADMISSIONS_CONTROL */
}

void
admissions_control_subtract(uint64_t admissions_estimate)
{
#ifdef ADMISSIONS_CONTROL
	/* Assumption: Should never underflow */
	if (unlikely(admissions_estimate > admissions_control_admitted)) panic("Admissions Estimate underflow\n");

	//atomic_fetch_sub(&admissions_control_admitted, admissions_estimate);

#ifdef LOG_ADMISSIONS_CONTROL
	debuglog("Runtime Admitted: %lu / %lu\n", admissions_control_admitted, admissions_control_capacity);
#endif

#endif /* ADMISSIONS_CONTROL */
}

uint64_t
admissions_control_calculate_estimate(uint64_t estimated_execution, uint64_t relative_deadline)
{
#ifdef ADMISSIONS_CONTROL
	assert(relative_deadline != 0);
	uint64_t admissions_estimate = (estimated_execution * (uint64_t)ADMISSIONS_CONTROL_GRANULARITY)
	                               / relative_deadline;
	if (admissions_estimate == 0)
		panic("Ratio of Deadline to Execution time cannot exceed %d\n", ADMISSIONS_CONTROL_GRANULARITY);

	return admissions_estimate;
#else
	return 0;
#endif
}

uint64_t
admissions_control_calculate_estimate_us(uint32_t estimated_execution_us, uint32_t relative_deadline_us)
{
#ifdef ADMISSIONS_CONTROL
	assert(relative_deadline_us != 0);
	return (uint64_t)((uint64_t)(estimated_execution_us * ADMISSIONS_CONTROL_GRANULARITY)) / relative_deadline_us;
#else
	return 0;
#endif
}

void
admissions_control_log_decision(uint64_t admissions_estimate, bool admitted)
{
#ifdef ADMISSIONS_CONTROL
#ifdef LOG_ADMISSIONS_CONTROL
	debuglog("Admitted: %lu, Capacity: %lu, Estimate: %lu, Admitted? %s\n", admissions_control_admitted,
	         admissions_control_capacity, admissions_estimate, admitted ? "yes" : "no");
#endif /* LOG_ADMISSIONS_CONTROL */
#endif /* ADMISSIONS_CONTROL */
}

uint64_t
admissions_control_decide(uint64_t admissions_estimate)
{
	uint64_t work_admitted = 1; /* Nominal non-zero value in case admissions control is disabled */

#ifdef ADMISSIONS_CONTROL
	if (unlikely(admissions_estimate == 0)) panic("Admissions estimate should never be zero");

	uint64_t total_admitted = atomic_load(&admissions_control_admitted);

	if (total_admitted + admissions_estimate >= admissions_control_capacity) {
		admissions_control_log_decision(admissions_estimate, false);
		work_admitted = 0;
	} else {
		admissions_control_log_decision(admissions_estimate, true);
		admissions_control_add(admissions_estimate);
		work_admitted = admissions_estimate;
	}
#endif /* ADMISSIONS_CONTROL */

	return work_admitted;
}
