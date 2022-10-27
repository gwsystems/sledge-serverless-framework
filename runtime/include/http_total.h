#pragma once

#include <stdatomic.h>
#include <stdint.h>

/*
 * Counts to track requests and responses
 * requests and 5XX (admissions control rejections) are only tracked by the listener core, so they are not
 * behind a compiler flag. 2XX and 4XX can be incremented by worker cores, so they are behind a flag because
 * of concerns about contention
 */
#ifdef HTTP_TOTAL_COUNTERS
extern _Atomic uint32_t http_total_requests;
extern _Atomic uint32_t http_total_5XX;
extern _Atomic uint32_t http_total_2XX;
extern _Atomic uint32_t http_total_4XX;
#endif

static inline void
http_total_init()
{
#ifdef HTTP_TOTAL_COUNTERS
	atomic_init(&http_total_requests, 0);
	atomic_init(&http_total_2XX, 0);
	atomic_init(&http_total_4XX, 0);
	atomic_init(&http_total_5XX, 0);
#endif
}

static inline void
http_total_increment_request()
{
#ifdef HTTP_TOTAL_COUNTERS
	atomic_fetch_add(&http_total_requests, 1);
#endif
}

static inline void
http_total_increment_response(int status_code)
{
#ifdef HTTP_TOTAL_COUNTERS
	/*if (status_code >= 200 && status_code <= 299) {
		atomic_fetch_add(&http_total_2XX, 1);
	} else if (status_code >= 400 && status_code <= 499) {
		atomic_fetch_add(&http_total_4XX, 1);
	} else if (status_code >= 500 && status_code <= 599) {
		atomic_fetch_add(&http_total_5XX, 1);
	}*/
#endif
}
