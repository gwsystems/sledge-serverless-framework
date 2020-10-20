#pragma once

#include <stdatomic.h>
#include <stdint.h>

/*
 * Counts to track requests and responses
 * requests and 5XX (admissions control rejections) are only tracked by the listener core, so they are not
 * behind a compiler flag. 2XX and 4XX can be incremented by worker cores, so they are behind a flag because
 * of concerns about contention
 */
extern _Atomic uint32_t http_total_requests;
extern _Atomic uint32_t http_total_5XX;

#ifdef LOG_TOTAL_REQS_RESPS
extern _Atomic uint32_t http_total_2XX;
extern _Atomic uint32_t http_total_4XX;
#endif

static inline void
http_total_init()
{
	atomic_init(&http_total_requests, 0);
	atomic_init(&http_total_5XX, 0);
#ifdef LOG_TOTAL_REQS_RESPS
	atomic_init(&http_total_2XX, 0);
	atomic_init(&http_total_4XX, 0);
#endif
}

static inline void
http_total_increment_request()
{
	atomic_fetch_add(&http_total_requests, 1);
}

static inline void
http_total_increment_2xx()
{
#ifdef LOG_TOTAL_REQS_RESPS
	atomic_fetch_add(&http_total_2XX, 1);
#endif
}

static inline void
http_total_increment_4XX()
{
#ifdef LOG_TOTAL_REQS_RESPS
	atomic_fetch_add(&http_total_4XX, 1);
#endif
}

static inline void
http_total_increment_5XX()
{
	atomic_fetch_add(&http_total_5XX, 1);
}
