#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "runtime.h" /* For runtime_pid */

/* Used to read process-level metrics associated with sledgert from procfs
 * The parsing behavior is based on prtstat -r
 */


enum PROC_STAT
{
	PROC_STAT_PID   = 0, /* Process ID */
	PROC_STAT_COMM  = 1, /* Process Name */
	PROC_STAT_STATE = 2, /* State */
	PROC_STAT_PPID,      /* Parent Process ID */
	PROC_STAT_PGRP,      /* Group ID */
	PROC_STAT_SESSION,   /* Session ID */
	PROC_STAT_TTY_NR,    /* ??? */
	PROC_STAT_TPGID,     /* ??? */
	PROC_STAT_FLAGS,     /* ??? */
	PROC_STAT_MINFLT,    /* Minor Page Faults */
	PROC_STAT_CMINFLT,   /* Minor Page Faults of children */
	PROC_STAT_MAJFLT,    /* Major Page Faults */
	PROC_STAT_CMAJFLT,   /* Major Page Faults of children */
	PROC_STAT_UTIME,     /* User Time */
	PROC_STAT_STIME,     /* System Time */
	PROC_STAT_CUTIME,    /* Child User Time */
	PROC_STAT_CSTIME,    /* Child System Time */
	PROC_STAT_PRIORITY,
	PROC_STAT_NICE,
	PROC_STAT_NUM_THREADS,
	PROC_STAT_ITREALVALUE,
	PROC_STAT_STARTTIME, /* Start Time */
	PROC_STAT_VSIZE,     /* Virtual Memory */
	PROC_STAT_RSS,
	PROC_STAT_RSSLIM,
	PROC_STAT_STARTCODE,
	PROC_STAT_ENDCODE,
	PROC_STAT_STARTSTACK,
	PROC_STAT_KSTKESP,
	PROC_STAT_KSTKEIP,
	PROC_STAT_WCHAN,
	PROC_STAT_NSWAP,
	PROC_STAT_CNSWAP,
	PROC_STAT_EXIT_SIGNAL,
	PROC_STAT_PROCESSOR,
	PROC_STAT_RT_PRIORITY,
	PROC_STAT_POLICY,
	PROC_STAT_DELAYACCR_BLKIO_TICKS,
	PROC_STAT_GUEST_TIME,
	PROC_STAT_CGUEST_TIME,
	PROC_STAT_COUNT
};

struct proc_stat_metrics {
	uint64_t minor_page_faults;
	uint64_t major_page_faults;
	uint64_t child_minor_page_faults;
	uint64_t child_major_page_faults;
	uint64_t user_time;
	uint64_t system_time;
	uint64_t guest_time;
};

static inline void
proc_stat_metrics_init(struct proc_stat_metrics *stat)
{
	assert(runtime_pid > 0);

	// Open sledgert's stat file in procfs
	char path[256];
	snprintf(path, 256, "/proc/%d/stat", runtime_pid);
	FILE *proc_stat = fopen(path, "r");

	/* Read stat file into in-memory buffer */
	char buf[BUFSIZ];
	fgets(buf, BUFSIZ, proc_stat);
	fclose(proc_stat);

	/* Parse into an array of tokens with indices aligning to the PROC_STAT enum */
	char *pos = NULL;
	char *proc_stat_values[PROC_STAT_COUNT];
	for (int i = 0; i < PROC_STAT_COUNT; i++) {
		char *tok           = i == 0 ? strtok_r(buf, " ", &pos) : strtok_r(NULL, " ", &pos);
		proc_stat_values[i] = tok;
	}

	/* Fill the proc_state_metrics struct with metrics of interest */
	/* Minor Page Faults, Major Page Faults, Vsize, User, System, Guest, Uptime */
	stat->minor_page_faults       = strtoul(proc_stat_values[PROC_STAT_MINFLT], NULL, 10);
	stat->major_page_faults       = strtoul(proc_stat_values[PROC_STAT_MAJFLT], NULL, 10);
	stat->child_minor_page_faults = strtoul(proc_stat_values[PROC_STAT_CMINFLT], NULL, 10);
	stat->child_major_page_faults = strtoul(proc_stat_values[PROC_STAT_CMAJFLT], NULL, 10);
	stat->user_time               = strtoul(proc_stat_values[PROC_STAT_UTIME], NULL, 10);
	stat->system_time             = strtoul(proc_stat_values[PROC_STAT_STIME], NULL, 10);
	stat->guest_time              = strtoul(proc_stat_values[PROC_STAT_GUEST_TIME], NULL, 10);
}
