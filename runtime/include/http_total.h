#pragma once

#include <stdatomic.h>
#include <stdint.h>

/*
 * Counts to track requests and responses
 * requests and 5XX (admissions control rejections) are only tracked by the listener core, so they are not
 * behind a compiler flag. 2XX and 4XX can be incremented by worker cores, so they are behind a flag because
 * of concerns about contention
 */
extern _Atomic uint32_t http_total_requests;
extern _Atomic uint32_t http_total_5XX;

#ifdef LOG_TOTAL_REQS_RESPS
extern _Atomic uint32_t http_total_2XX;
extern _Atomic uint32_t http_total_4XX;
#endif

void http_total_init();
void http_total_increment_request();
void http_total_increment_2xx();
void http_total_increment_4XX();
void http_total_increment_5XX();
