#pragma once

#include <stdint.h>
#include <stddef.h>

#include "admissions_info.h"
#include "module.h"
#include "route_metrics.h"

/* Assumption: entrypoint is always _start. This should be enhanced later */
struct route {
	char                *route;
	struct route_metrics metrics;
	struct module       *module;
	/* HTTP State */
	uint32_t               relative_deadline_us;
	uint64_t               relative_deadline; /* cycles */
	size_t                 response_size;
	char                  *response_content_type;
	struct admissions_info admissions_info;
};
