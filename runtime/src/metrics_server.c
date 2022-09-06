#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "admissions_control.h"
#include "debuglog.h"
#include "http.h"
#include "http_total.h"
#include "metrics_server.h"
#include "proc_stat.h"
#include "runtime.h"
#include "sandbox_total.h"
#include "sandbox_state.h"
#include "tcp_server.h"

/* We run threads on the "reserved OS core" using blocking semantics */
#define METRICS_SERVER_CORE_ID 0
#define METRICS_SERVER_PORT    1776

struct metrics_server metrics_server;
static void          *metrics_server_handler(void *arg);

extern void metrics_server_route_level_metrics_render(FILE *ostream);

void
metrics_server_init()
{
	metrics_server.tag = EPOLL_TAG_METRICS_SERVER_SOCKET;
	tcp_server_init(&metrics_server.tcp, METRICS_SERVER_PORT);
	int rc = tcp_server_listen(&metrics_server.tcp);
	assert(rc == 0);

	/* Configure pthread attributes to pin metrics server threads to CPU 0 */
	pthread_attr_init(&metrics_server.thread_settings);
	cpu_set_t cs;
	CPU_ZERO(&cs);
	CPU_SET(METRICS_SERVER_CORE_ID, &cs);
	pthread_attr_setaffinity_np(&metrics_server.thread_settings, sizeof(cpu_set_t), &cs);
}

int
metrics_server_close()
{
	return tcp_server_close(&metrics_server.tcp);
}

void
metrics_server_thread_spawn(int client_socket)
{
	/* Fire and forget, so we don't save the thread handles */
	pthread_t metrics_server_thread;
	int       rc = pthread_create(&metrics_server_thread, &metrics_server.thread_settings, metrics_server_handler,
	                              (void *)(long)client_socket);

	if (rc != 0) {
		debuglog("Metrics Server failed to spawn pthread with %s\n", strerror(rc));
		close(client_socket);
	}
}

static void *
metrics_server_handler(void *arg)
{
	/* Intermediate cast to integral value of 64-bit width to silence compiler nits */
	int client_socket = (int)(long)arg;

	/* Duplicate fd so fclose doesn't close the actual client_socket */
	int   temp_fd  = dup(client_socket);
	FILE *req_body = fdopen(temp_fd, "r");

	/* Basic L7 routing to filter out favicon requests */
	char http_status_code_buf[256];
	fgets(http_status_code_buf, 256, req_body);
	fclose(req_body);

	if (strncmp(http_status_code_buf, "GET /metrics HTTP", 10) != 0) {
		write(client_socket, http_header_build(404), http_header_len(404));
		close(client_socket);
		pthread_exit(NULL);
	}

	int rc = 0;

	char  *ostream_base = NULL;
	size_t ostream_size = 0;
	FILE  *ostream      = open_memstream(&ostream_base, &ostream_size);
	assert(ostream != NULL);

#ifdef HTTP_TOTAL_COUNTERS
	uint32_t total_reqs = atomic_load(&http_total_requests);
	uint32_t total_5XX  = atomic_load(&http_total_5XX);
	uint32_t total_2XX  = atomic_load(&http_total_2XX);
	uint32_t total_4XX  = atomic_load(&http_total_4XX);
#endif

	uint64_t total_sandboxes = atomic_load(&sandbox_total);

#ifdef SANDBOX_STATE_TOTALS
	uint32_t total_sandboxes_uninitialized = atomic_load(&sandbox_state_totals[SANDBOX_UNINITIALIZED]);
	uint32_t total_sandboxes_allocated     = atomic_load(&sandbox_state_totals[SANDBOX_ALLOCATED]);
	uint32_t total_sandboxes_initialized   = atomic_load(&sandbox_state_totals[SANDBOX_INITIALIZED]);
	uint32_t total_sandboxes_runnable      = atomic_load(&sandbox_state_totals[SANDBOX_RUNNABLE]);
	uint32_t total_sandboxes_preempted     = atomic_load(&sandbox_state_totals[SANDBOX_PREEMPTED]);
	uint32_t total_sandboxes_running_sys   = atomic_load(&sandbox_state_totals[SANDBOX_RUNNING_SYS]);
	uint32_t total_sandboxes_running_user  = atomic_load(&sandbox_state_totals[SANDBOX_RUNNING_USER]);
	uint32_t total_sandboxes_interrupted   = atomic_load(&sandbox_state_totals[SANDBOX_INTERRUPTED]);
	uint32_t total_sandboxes_asleep        = atomic_load(&sandbox_state_totals[SANDBOX_ASLEEP]);
	uint32_t total_sandboxes_returned      = atomic_load(&sandbox_state_totals[SANDBOX_RETURNED]);
	uint32_t total_sandboxes_complete      = atomic_load(&sandbox_state_totals[SANDBOX_COMPLETE]);
	uint32_t total_sandboxes_error         = atomic_load(&sandbox_state_totals[SANDBOX_ERROR]);
#endif

#ifdef ADMISSIONS_CONTROL
	uint32_t work_admitted            = atomic_load(&admissions_control_admitted);
	double   work_admitted_percentile = (double)work_admitted / admissions_control_capacity * 100;
#endif

#ifdef PROC_STAT_METRICS
	struct proc_stat_metrics stat;
	proc_stat_metrics_init(&stat);
#endif

	fprintf(ostream, "HTTP/1.1 200 OK\r\n\r\n");

#ifdef PROC_STAT_METRICS
	fprintf(ostream, "# TYPE os_proc_major_page_faults counter\n");
	fprintf(ostream, "os_proc_major_page_faults: %lu\n", stat.major_page_faults);

	fprintf(ostream, "# TYPE os_proc_minor_page_faults counter\n");
	fprintf(ostream, "os_proc_minor_page_faults: %lu\n", stat.minor_page_faults);

	fprintf(ostream, "# TYPE os_proc_child_major_page_faults counter\n");
	fprintf(ostream, "os_proc_child_major_page_faults: %lu\n", stat.child_major_page_faults);

	fprintf(ostream, "# TYPE os_proc_child_minor_page_faults counter\n");
	fprintf(ostream, "os_proc_child_minor_page_faults: %lu\n", stat.child_minor_page_faults);

	fprintf(ostream, "# TYPE os_proc_user_time counter\n");
	fprintf(ostream, "os_proc_user_time: %lu\n", stat.user_time);

	fprintf(ostream, "# TYPE os_proc_sys_time counter\n");
	fprintf(ostream, "os_proc_sys_time: %lu\n", stat.system_time);

	fprintf(ostream, "# TYPE os_proc_guest_time counter\n");
	fprintf(ostream, "os_proc_guest_time: %lu\n", stat.guest_time);
#endif /* PROC_STAT_METRICS */

#ifdef ADMISSIONS_CONTROL
	fprintf(ostream, "# TYPE work_admitted_percentile gauge\n");
	fprintf(ostream, "work_admitted_percentile: %f\n", work_admitted_percentile);
#endif

#ifdef HTTP_TOTAL_COUNTERS
	fprintf(ostream, "# TYPE total_requests counter\n");
	fprintf(ostream, "total_requests: %d\n", total_reqs);

	fprintf(ostream, "# TYPE total_2XX counter\n");
	fprintf(ostream, "total_2XX: %d\n", total_2XX);

	fprintf(ostream, "# TYPE total_4XX counter\n");
	fprintf(ostream, "total_4XX: %d\n", total_4XX);

	fprintf(ostream, "# TYPE total_5XX counter\n");
	fprintf(ostream, "total_5XX: %d\n", total_5XX);
#endif

	metrics_server_route_level_metrics_render(ostream);

	fprintf(ostream, "# TYPE total_sandboxes counter\n");
	fprintf(ostream, "total_sandboxes: %lu\n", total_sandboxes);

#ifdef SANDBOX_STATE_TOTALS
	fprintf(ostream, "# TYPE total_sandboxes_uninitialized gauge\n");
	fprintf(ostream, "total_sandboxes_uninitialized: %d\n", total_sandboxes_uninitialized);

	fprintf(ostream, "# TYPE total_sandboxes_allocated gauge\n");
	fprintf(ostream, "total_sandboxes_allocated: %d\n", total_sandboxes_allocated);

	fprintf(ostream, "# TYPE total_sandboxes_initialized gauge\n");
	fprintf(ostream, "total_sandboxes_initialized: %d\n", total_sandboxes_initialized);

	fprintf(ostream, "# TYPE total_sandboxes_runnable gauge\n");
	fprintf(ostream, "total_sandboxes_runnable: %d\n", total_sandboxes_runnable);

	fprintf(ostream, "# TYPE total_sandboxes_preempted gauge\n");
	fprintf(ostream, "total_sandboxes_preempted: %d\n", total_sandboxes_preempted);

	fprintf(ostream, "# TYPE total_sandboxes_running_sys gauge\n");
	fprintf(ostream, "total_sandboxes_running_sys: %d\n", total_sandboxes_running_sys);

	fprintf(ostream, "# TYPE total_sandboxes_running_user gauge\n");
	fprintf(ostream, "total_sandboxes_running_user: %d\n", total_sandboxes_running_user);

	fprintf(ostream, "# TYPE total_sandboxes_interrupted gauge\n");
	fprintf(ostream, "total_sandboxes_interrupted: %d\n", total_sandboxes_interrupted);

	fprintf(ostream, "# TYPE total_sandboxes_asleep gauge\n");
	fprintf(ostream, "total_sandboxes_asleep: %d\n", total_sandboxes_asleep);

	fprintf(ostream, "# TYPE total_sandboxes_returned gauge\n");
	fprintf(ostream, "total_sandboxes_returned: %d\n", total_sandboxes_returned);

	fprintf(ostream, "# TYPE total_sandboxes_complete gauge\n");
	fprintf(ostream, "total_sandboxes_complete: %d\n", total_sandboxes_complete);

	fprintf(ostream, "# TYPE total_sandboxes_error gauge\n");
	fprintf(ostream, "total_sandboxes_error: %d\n", total_sandboxes_error);
#endif

	fflush(ostream);
	assert(ostream_size > 0);
	rc = fclose(ostream);
	assert(rc == 0);

	/* Closing the memstream does not close the generated buffer */
	ssize_t nwritten = write(client_socket, ostream_base, ostream_size);
	assert(nwritten == ostream_size);

	free(ostream_base);
	ostream_size = 0;

	close(client_socket);

	pthread_exit(NULL);
}
