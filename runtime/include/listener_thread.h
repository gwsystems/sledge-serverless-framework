#pragma once

#include <stdbool.h>
#include <stdnoreturn.h>

#include "http_session.h"
#include "module.h"
#include "ck_ring.h"
#include "sandbox_state.h"
#include "dbf.h"

#define LISTENER_THREAD_CORE_ID   1
#define LISTENER_THREAD_RING_SIZE 10240 /* the acutal size becomes 255 */

struct comm_with_worker {
	ck_ring_t      worker_ring;
	struct message worker_ring_buffer[LISTENER_THREAD_RING_SIZE];
	int            worker_idx;
}; // __attribute__((aligned(CACHE_PAD))); ///// TODO: this necessary?

CK_RING_PROTOTYPE(message, message)

extern pthread_t                listener_thread_id;
extern int                      listener_thread_epoll_file_descriptor;
extern struct comm_with_worker *comm_from_workers;
extern struct comm_with_worker *comm_from_workers_extra;
extern struct comm_with_worker *comm_to_workers;

void           listener_thread_initialize(void);
noreturn void *listener_thread_main(void *dummy);
void           listener_thread_register_http_session(struct http_session *http);

/**
 * Used to determine if running in the context of a listener thread
 * @returns true if listener. false if not (probably a worker)
 */
static inline bool
listener_thread_is_running()
{
	return pthread_self() == listener_thread_id;
}

static inline void
comm_with_workers_init(struct comm_with_worker *comm_with_workers)
{
	assert(comm_with_workers);

	for (int worker_idx = 0; worker_idx < runtime_worker_threads_count; worker_idx++) {
		ck_ring_init(&comm_with_workers[worker_idx].worker_ring, LISTENER_THREAD_RING_SIZE);
		comm_with_workers[worker_idx].worker_idx = worker_idx;
	}
}
