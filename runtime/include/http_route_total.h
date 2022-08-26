#pragma once

#include <stdatomic.h>

#ifdef HTTP_ROUTE_TOTAL_COUNTERS
struct http_route_total {
	atomic_ulong total_requests;
	atomic_ulong total_2XX;
	atomic_ulong total_4XX;
	atomic_ulong total_5XX;
};
#else
struct http_route_total {
};
#endif

static inline void
http_route_total_init(struct http_route_total *rm)
{
#ifdef HTTP_ROUTE_TOTAL_COUNTERS
	atomic_init(&rm->total_requests, 0);
	atomic_init(&rm->total_2XX, 0);
	atomic_init(&rm->total_4XX, 0);
	atomic_init(&rm->total_5XX, 0);
#endif
}

static inline void
http_route_total_increment_request(struct http_route_total *rm)
{
#ifdef HTTP_ROUTE_TOTAL_COUNTERS
	atomic_fetch_add(&rm->total_requests, 1);
#endif
}

static inline void
http_route_total_increment(struct http_route_total *rm, int status_code)
{
#ifdef HTTP_ROUTE_TOTAL_COUNTERS
	if (status_code >= 200 && status_code <= 299) {
		atomic_fetch_add(&rm->total_2XX, 1);
	} else if (status_code >= 400 && status_code <= 499) {
		atomic_fetch_add(&rm->total_4XX, 1);
	} else if (status_code >= 500 && status_code <= 599) {
		atomic_fetch_add(&rm->total_5XX, 1);
	}
#endif
}
