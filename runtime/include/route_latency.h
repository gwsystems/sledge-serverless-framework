#pragma once

#include <stdint.h>

#include "perf_window.h"

static inline void
route_latency_init(struct perf_window *route_latency)
{
#ifdef ROUTE_LATENCY
	perf_window_initialize(route_latency);
#endif
}

static inline uint64_t
route_latency_get(struct perf_window *route_latency, uint8_t percentile, int precomputed_index)
{
#ifdef ROUTE_LATENCY
	lock_node_t node = {};
	lock_lock(&route_latency->lock, &node);
	uint64_t res = perf_window_get_percentile(route_latency, percentile, precomputed_index);
	lock_unlock(&route_latency->lock, &node);
	return res;
#endif
}

static inline void
route_latency_add(struct perf_window *route_latency, uint64_t value)
{
#ifdef ROUTE_LATENCY
	lock_node_t node = {};
	lock_lock(&route_latency->lock, &node);
	perf_window_add(route_latency, value);
	lock_unlock(&route_latency->lock, &node);
#endif
}
