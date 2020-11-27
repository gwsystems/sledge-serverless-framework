#pragma once

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define panic(fmt, ...)                                                                                           \
	{                                                                                                         \
		fprintf(stderr, "C: %02d, T: 0x%lx, F: %s> PANIC! \n\t" fmt "\n", sched_getcpu(), pthread_self(), \
		        __func__, ##__VA_ARGS__);                                                                 \
		exit(EXIT_FAILURE);                                                                               \
	}

#define panic_err() panic("%s", strerror(errno));
