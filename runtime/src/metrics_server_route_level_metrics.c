#include <stdatomic.h>
#include <stdint.h>

#include "perf_window.h"
#include "tenant_functions.h"

void
render_routes(struct route *route, void *arg_one, void *arg_two)
{
#ifdef HTTP_ROUTE_TOTAL_COUNTERS
	FILE          *ostream = (FILE *)arg_one;
	struct tenant *tenant  = (struct tenant *)arg_two;

#ifdef ROUTE_LATENCY
	uint64_t latency_p50 = route_latency_get(&route->latency, 50, 0);
	uint64_t latency_p90 = route_latency_get(&route->latency, 90, 0);
#endif /* ROUTE_LATENCY */

	uint64_t total_requests = atomic_load(&route->metrics.total_requests);
	uint64_t total_2XX      = atomic_load(&route->metrics.total_2XX);
	uint64_t total_4XX      = atomic_load(&route->metrics.total_4XX);
	uint64_t total_5XX      = atomic_load(&route->metrics.total_5XX);

	// Strip leading /
	const char *route_label = &route->route[1];

	fprintf(ostream, "# TYPE %s_%s_total_requests counter\n", tenant->name, route_label);
	fprintf(ostream, "%s_%s_total_requests: %lu\n", tenant->name, route_label, total_requests);

	fprintf(ostream, "# TYPE %s_%s_total_2XX counter\n", tenant->name, route_label);
	fprintf(ostream, "%s_%s_total_2XX: %lu\n", tenant->name, route_label, total_2XX);

	fprintf(ostream, "# TYPE %s_%s_total_4XX counter\n", tenant->name, route_label);
	fprintf(ostream, "%s_%s_total_4XX: %lu\n", tenant->name, route_label, total_4XX);

	fprintf(ostream, "# TYPE %s_%s_total_5XX counter\n", tenant->name, route_label);
	fprintf(ostream, "%s_%s_total_5XX: %lu\n", tenant->name, route_label, total_5XX);

#ifdef ROUTE_LATENCY
	fprintf(ostream, "# TYPE %s_%s_latency_us_p50 gauge\n", tenant->name, route_label);
	fprintf(ostream, "%s_%s_latency_us_p50: %lu\n", tenant->name, route_label,
	        latency_p50 / runtime_processor_speed_MHz);

	fprintf(ostream, "# TYPE %s_%s_latency_us_p90 gauge\n", tenant->name, route_label);
	fprintf(ostream, "%s_%s_latency_us_p90: %lu\n", tenant->name, route_label,
	        latency_p90 / runtime_processor_speed_MHz);
#endif /* ROUTE_LATENCY */

#endif /* HTTP_ROUTE_TOTAL_COUNTERS */
}

void
render_tenant_routers(struct tenant *tenant, void *arg_one, void *arg_two)
{
	FILE *ostream = (FILE *)arg_one;
	char *name    = tenant->name;

	http_router_foreach(&tenant->router, render_routes, ostream, tenant);
}

void
metrics_server_route_level_metrics_render(FILE *ostream)
{
#ifdef HTTP_ROUTE_TOTAL_COUNTERS
	tenant_database_foreach(render_tenant_routers, ostream, NULL);
#endif
}
