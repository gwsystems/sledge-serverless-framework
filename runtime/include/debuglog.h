#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

extern int32_t debuglog_file_descriptor;

#ifdef LOG_TO_FILE
#ifndef DEBUG
#error LOG_TO_FILE is only valid if in DEBUG mode
#endif /* DEBUG */
#endif /* LOG_TO_FILE */

/**
 * debuglog is a macro that behaves based on the macros DEBUG and LOG_TO_FILE
 * If DEBUG is not set, debuglog does nothing
 * If DEBUG is set and LOG_TO_FILE is set, debuglog prints to the logfile defined in debuglog_file_descriptor
 * If DEBUG is set and LOG_TO_FILE is not set, debuglog prints to STDOUT
 */
#ifdef DEBUG
#ifdef LOG_TO_FILE
#define debuglog(fmt, ...)                                                                                           \
	dprintf(debuglog_file_descriptor, "C: %02d, T: 0x%lx, F: %s> \n\t" fmt "\n", sched_getcpu(), pthread_self(), \
	        __func__, ##__VA_ARGS__);
#else /* !LOG_TO_FILE */
#define debuglog(fmt, ...)                                                                                   \
	fprintf(stderr, "C: %02d, T: 0x%lx, F: %s> \n\t" fmt "\n", sched_getcpu(), pthread_self(), __func__, \
	        ##__VA_ARGS__);
#endif /* LOG_TO_FILE */
#else  /* !DEBUG */
#define debuglog(fmt, ...)
#endif /* DEBUG */
