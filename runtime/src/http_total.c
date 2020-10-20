#include <stdint.h>

#include "debuglog.h"
#include "http_total.h"

/* 2XX + 4XX should equal sandboxes */
/* Listener Core Bookkeeping */
_Atomic uint32_t http_total_requests = 0;
_Atomic uint32_t http_total_5XX      = 0;

#ifdef LOG_TOTAL_REQS_RESPS
_Atomic uint32_t http_total_2XX = 0;
_Atomic uint32_t http_total_4XX = 0;
#endif

void
http_total_log()
{
	uint32_t total_reqs = atomic_load(&http_total_requests);
	uint32_t total_5XX  = atomic_load(&http_total_5XX);

#ifdef LOG_TOTAL_REQS_RESPS
	uint32_t total_2XX = atomic_load(&http_total_2XX);
	uint32_t total_4XX = atomic_load(&http_total_4XX);

	int64_t total_responses      = total_2XX + total_4XX + total_5XX;
	int64_t outstanding_requests = (int64_t)total_reqs - total_responses;

	debuglog("Requests: %u (%ld outstanding)\n\tResponses: %ld\n\t\t2XX: %u\n\t\t4XX: %u\n\t\t5XX: %u\n",
	         total_reqs, outstanding_requests, total_responses, total_2XX, total_4XX, total_5XX);
#else
	debuglog("Requests: %u\n\tResponses:\n\t\t\t5XX: %u\n", total_reqs, total_5XX);
#endif
};
