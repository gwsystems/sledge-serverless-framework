#include "http_session_perf_log.h"

FILE *http_session_perf_log = NULL;

/**
 * Prints key performance metrics for a http_session to http_session_perf_log
 * This is defined by an environment variable
 * @param http_session
 */
void
http_session_perf_log_print_entry(struct http_session *http_session)
{
	/* If the log was not defined by an environment variable, early out */
	if (http_session_perf_log == NULL) return;

	const uint64_t receive_duration = http_session->request_downloaded_timestamp
	                                  - http_session->request_arrival_timestamp;
	const uint64_t sent_duration = http_session->response_sent_timestamp - http_session->response_takeoff_timestamp;
	const uint64_t total_lifetime = http_session->response_sent_timestamp - http_session->request_arrival_timestamp;

	fprintf(http_session_perf_log, "%s,%s,%u,%lu,%lu,%lu,%lu,%lu,%u\n", http_session->tenant->name,
	        http_session->http_request.full_url, http_session->state, http_session->response_header_written,
	        http_session->response_buffer_written, receive_duration, sent_duration, total_lifetime,
	        runtime_processor_speed_MHz);
}
