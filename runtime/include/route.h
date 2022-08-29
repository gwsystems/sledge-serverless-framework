#pragma once

#include <stdint.h>
#include <stddef.h>

#include "admissions_info.h"
#include "module.h"
#include "http_route_total.h"

/* Assumption: entrypoint is always _start. This should be enhanced later */
struct route {
	char	           *route;
	struct http_route_total metrics;
	struct module          *module;
	/* HTTP State */
	uint32_t               relative_deadline_us;
	uint64_t               relative_deadline; /* cycles */
	char	          *response_content_type;
	struct admissions_info admissions_info;
};
