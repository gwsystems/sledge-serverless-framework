#pragma once

#include "pretty_print.h"
#include "runtime.h"
#include "http_session.h"

extern FILE                *http_session_perf_log;
typedef struct http_session http_session;
void                        http_session_perf_log_print_entry(struct http_session *http_session);

/**
 * @brief Prints headers for the per-session perf logs
 */
static inline void
http_session_perf_log_print_header()
{
	if (http_session_perf_log == NULL) { perror("http_session perf log"); }
	fprintf(http_session_perf_log,
	        "tenant,route,state,header_len,resp_body_len,receive_duration,sent_duration,total_lifetime,proc_MHz\n");
}

static inline void
http_session_perf_log_init()
{
	char *http_session_perf_log_path = getenv("SLEDGE_HTTP_SESSION_PERF_LOG");
	if (http_session_perf_log_path != NULL) {
		pretty_print_key_value("HTTP Session Performance Log", "%s\n", http_session_perf_log_path);
		http_session_perf_log = fopen(http_session_perf_log_path, "w");
		if (http_session_perf_log == NULL) perror("http_session_perf_log_init\n");
		http_session_perf_log_print_header();
	} else {
		pretty_print_key_disabled("HTTP Session Performance Log");
	}
}

static inline void
http_session_perf_log_cleanup()
{
	if (http_session_perf_log != NULL) {
		fflush(http_session_perf_log);
		fclose(http_session_perf_log);
	}
}
