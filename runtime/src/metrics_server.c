#include <stdlib.h>
#include <unistd.h>

#include "admissions_control.h"
#include "tcp_server.h"
#include "http_total.h"
#include "sandbox_total.h"
#include "sandbox_state.h"

struct tcp_server metrics_server;

void
metrics_server_init()
{
	tcp_server_init(&metrics_server, 1776);
}

int
metrics_server_listen()
{
	return tcp_server_listen(&metrics_server);
}

int
metrics_server_close()
{
	return tcp_server_close(&metrics_server);
}

void
metrics_server_handler(int client_socket)
{
	int rc = 0;

	char  *ostream_base = NULL;
	size_t ostream_size = 0;
	FILE  *ostream      = open_memstream(&ostream_base, &ostream_size);
	assert(ostream != NULL);

	uint32_t total_reqs = atomic_load(&http_total_requests);
	uint32_t total_5XX  = atomic_load(&http_total_5XX);

#ifdef LOG_TOTAL_REQS_RESPS
	uint32_t total_2XX = atomic_load(&http_total_2XX);
	uint32_t total_4XX = atomic_load(&http_total_4XX);
#endif

	uint32_t total_sandboxes = atomic_load(&sandbox_total);

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

	fprintf(ostream, "HTTP/1.1 200 OK\r\n\r\n");

	fprintf(ostream, "# TYPE total_requests counter\n");
	fprintf(ostream, "total_requests: %d\n", total_reqs);

#ifdef ADMISSIONS_CONTROL
	fprintf(ostream, "# TYPE work_admitted_percentile gauge\n");
	fprintf(ostream, "work_admitted_percentile: %f\n", work_admitted_percentile);
#endif

	fprintf(ostream, "# TYPE total_5XX counter\n");
	fprintf(ostream, "total_5XX: %d\n", total_5XX);

#ifdef LOG_TOTAL_REQS_RESPS
	fprintf(ostream, "# TYPE total_2XX counter\n");
	fprintf(ostream, "total_2XX: %d\n", total_2XX);

	fprintf(ostream, "# TYPE total_4XX counter\n");
	fprintf(ostream, "total_4XX: %d\n", total_4XX);
#endif

	// This global is padded by 1 for error handling, so decrement here for true value
	fprintf(ostream, "# TYPE total_sandboxes counter\n");
	fprintf(ostream, "total_sandboxes: %d\n", total_sandboxes - 1);

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
	write(client_socket, ostream_base, ostream_size);

	rc = fclose(ostream);
	assert(rc == 0);

	free(ostream_base);
	ostream_size = 0;

	close(client_socket);
}
