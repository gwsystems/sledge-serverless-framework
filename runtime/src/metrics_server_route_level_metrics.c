#include <stdatomic.h>
#include <stdint.h>

#include "perf_window.h"
#include "tenant_functions.h"

// tenant_database_foreach_cb_t

static const int p50_idx = perf_window_capacity * 50 / 100;
static const int p90_idx = perf_window_capacity * 90 / 100;

void
render_routes(struct route *route, void *arg_one, void *arg_two)
{
#ifdef HTTP_ROUTE_TOTAL_COUNTERS
	FILE          *ostream = (FILE *)arg_one;
	struct tenant *tenant  = (struct tenant *)arg_two;

#ifdef ADMISSIONS_CONTROL
	uint64_t latency_p50 = perf_window_get_percentile(&route->admissions_info.perf_window, 50, p50_idx);
	uint64_t latency_p90 = perf_window_get_percentile(&route->admissions_info.perf_window, 90, p90_idx);
#endif

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

#ifdef ADMISSIONS_CONTROL
	fprintf(ostream, "# TYPE %s_%s_latency_p50 gauge\n", tenant->name, route_label);
	fprintf(ostream, "%s_%s_latency_p50: %lu\n", tenant->name, route_label, latency_p50);

	fprintf(ostream, "# TYPE %s_%s_latency_p90 gauge\n", tenant->name, route_label);
	fprintf(ostream, "%s_%s_latency_p90: %lu\n", tenant->name, route_label, latency_p90);
#endif

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
