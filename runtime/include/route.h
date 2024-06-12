#pragma once

#include <stddef.h>
#include <stdint.h>

#include "execution_histogram.h"
#include "http_route_total.h"
#include "module.h"
#include "perf_window.h"

struct regression_model {
	double   bias;
	double   scale;
	uint32_t num_of_param;
	double   beta1;
	double   beta2;
};

/* Assumption: entrypoint is always _start. This should be enhanced later */
struct route {
	char                   *route;
	struct http_route_total metrics;
	struct module          *module;
	/* HTTP State */
	uint32_t                   relative_deadline_us;
	uint64_t                   relative_deadline; /* cycles */
	char                      *response_content_type;
	struct execution_histogram execution_histogram;
	struct perf_window         latency;
	struct module             *module_proprocess;
	struct regression_model    regr_model;
};
