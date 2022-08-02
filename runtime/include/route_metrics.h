#pragma once

#include <stdatomic.h>

struct route_metrics {
	atomic_ulong total_requests;
	atomic_ulong total_2XX;
	atomic_ulong total_4XX;
	atomic_ulong total_5XX;
};

static inline void
route_metrics_init(struct route_metrics *rm)
{
	atomic_init(&rm->total_requests, 0);
	atomic_init(&rm->total_2XX, 0);
	atomic_init(&rm->total_4XX, 0);
	atomic_init(&rm->total_5XX, 0);
}

static inline void
route_metrics_increment_request(struct route_metrics *rm)
{
	atomic_fetch_add(&rm->total_requests, 1);
}

static inline void
route_metrics_increment_2XX(struct route_metrics *rm)
{
	atomic_fetch_add(&rm->total_2XX, 1);
}

static inline void
route_metrics_increment_4XX(struct route_metrics *rm)
{
	atomic_fetch_add(&rm->total_4XX, 1);
}

static inline void
route_metrics_increment_5XX(struct route_metrics *rm)
{
	atomic_fetch_add(&rm->total_5XX, 1);
}

static inline void
route_metrics_increment(struct route_metrics *rm, int status_code)
{
	if (status_code >= 200 && status_code <= 299) {
		route_metrics_increment_2XX(rm);
	} else if (status_code >= 400 && status_code <= 499) {
		route_metrics_increment_4XX(rm);
	} else if (status_code >= 500 && status_code <= 599) {
		route_metrics_increment_5XX(rm);
	}
}
