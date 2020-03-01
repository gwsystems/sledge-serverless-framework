#include <runtime.h>
#include <sys/mman.h>
#include <types.h>
#include <sandbox.h>
#include <arch/context.h>
#include <module.h>
#include <signal.h>
#include <pthread.h>
#include <sched.h>
#include <softint.h>
#include <uv.h>
#include <http_api.h>

struct deque_sandbox *global_deque;
pthread_mutex_t       global_deque_mutex = PTHREAD_MUTEX_INITIALIZER;
int                   epoll_file_descriptor;

__thread static struct ps_list_head local_run_queue;
__thread static struct ps_list_head local_completion_queue;

// current sandbox that is active..
__thread sandbox_t *current_sandbox = NULL;

// context pointer to switch to when this thread gets a SIGUSR1
__thread arch_context_t *next_context = NULL;

// context of the runtime thread before running sandboxes or to resume its "main".
__thread arch_context_t base_context;

// libuv i/o loop handle per sandboxing thread!
__thread uv_loop_t uvio;

/**
 * Append the sandbox to the local_run_queueueue
 * @param s sandbox to add
 */
static inline void
sandbox_local_run(struct sandbox *s)
{
	assert(ps_list_singleton_d(s));
	//	fprintf(stderr, "(%d,%lu) %s: run %p, %s\n", sched_getcpu(), pthread_self(), __func__, s, s->mod->name);
	ps_list_head_append_d(&local_run_queue, s);
}

static inline int
sandbox_pull(void)
{
	int n = 0;

	while (n < SBOX_PULL_MAX) {
		sbox_request_t *s = sandbox_deque_steal();

		if (!s) break;
		struct sandbox *sb = sandbox_alloc(s->mod, s->args, s->sock, s->addr);
		assert(sb);
		free(s);
		sb->state = SANDBOX_RUNNABLE;
		sandbox_local_run(sb);
		n++;
	}

	return n;
}

static __thread unsigned int in_callback;

void
sandbox_io_nowait(void)
{
	// non-zero if more callbacks are expected
	in_callback = 1;
	int n = uv_run(runtime_uvio(), UV_RUN_NOWAIT), i = 0;
	while (n > 0) {
		n--;
		uv_run(runtime_uvio(), UV_RUN_NOWAIT);
	}
	in_callback = 0;
	// zero, so there is nothing (don't block!)
}

struct sandbox *
sandbox_schedule(int interrupt)
{
	struct sandbox *s = NULL;
	if (ps_list_head_empty(&local_run_queue)) {
		// this is in an interrupt context, don't steal work here!
		if (interrupt) return NULL;
		if (sandbox_pull() == 0) {
			// debuglog("[null: null]\n");
			return NULL;
		}
	}

	s = ps_list_head_first_d(&local_run_queue, struct sandbox);

	assert(s->state != SANDBOX_RETURNED);
	// round-robin
	ps_list_rem_d(s);
	ps_list_head_append_d(&local_run_queue, s);
	debuglog("[%p: %s]\n", s, s->mod->name);

	return s;
}

/**
 * @brief Remove and free n requests from the completion queue
 * @param number_to_free The number of requests to free
 * @return void
 */
static inline void
sandbox_local_free(unsigned int number_to_free)
{
	for (int i = 0; i < number_to_free; i++){
		if (ps_list_head_empty(&local_completion_queue)) break;
		struct sandbox *s = ps_list_head_first_d(&local_completion_queue, struct sandbox);
		if (!s) break;
		ps_list_rem_d(s);
		sandbox_free(s);
	}
}

struct sandbox *
sandbox_schedule_io(void)
{
	assert(sandbox_current() == NULL);
	sandbox_local_free(1);
	if (!in_callback) sandbox_io_nowait();

	softint_disable();
	struct sandbox *s = sandbox_schedule(0);
	softint_enable();
	assert(s == NULL || s->state == SANDBOX_RUNNABLE);

	return s;
}

void
sandbox_wakeup(sandbox_t *s)
{
	softint_disable();
	debuglog("[%p: %s]\n", s, s->mod->name);
	if (s->state != SANDBOX_BLOCKED) goto done;
	assert(s->state == SANDBOX_BLOCKED);
	assert(ps_list_singleton_d(s));
	s->state = SANDBOX_RUNNABLE;
	ps_list_head_append_d(&local_run_queue, s);
done:
	softint_enable();
}

void
sandbox_block(void)
{
	assert(in_callback == 0);
	softint_disable();
	struct sandbox *c = sandbox_current();
	ps_list_rem_d(c);
	c->state          = SANDBOX_BLOCKED;
	struct sandbox *s = sandbox_schedule(0);
	debuglog("[%p: %s, %p: %s]\n", c, c->mod->name, s, s ? s->mod->name : "");
	softint_enable();
	sandbox_switch(s);
}

void
sandbox_block_http(void)
{
#ifdef USE_HTTP_UVIO
#ifdef USE_HTTP_SYNC
	// realistically, we're processing all async I/O on this core when a sandbox blocks on http processing, not
	// great! if there is a way (TODO), perhaps RUN_ONCE and check if your I/O is processed, if yes, return else do
	// async block!
	uv_run(runtime_uvio(), UV_RUN_DEFAULT);
#else
	sandbox_block();
#endif
#else
	assert(0);
	// it should not be called if not using uvio for http
#endif
}

void __attribute__((noinline)) __attribute__((noreturn)) sandbox_switch_preempt(void)
{
	pthread_kill(pthread_self(), SIGUSR1);

	assert(0); // should not get here..
	while (true)
		;
}
static inline void
sandbox_local_stop(struct sandbox *s)
{
	ps_list_rem_d(s);
}

void
sandbox_local_end(struct sandbox *s)
{
	assert(ps_list_singleton_d(s));
	ps_list_head_append_d(&local_completion_queue, s);
}

void *
sandbox_run_func(void *data)
{
	arch_context_init(&base_context, 0, 0);

	ps_list_head_init(&local_run_queue);
	ps_list_head_init(&local_completion_queue);
	softint_off  = 0;
	next_context = NULL;
#ifndef PREEMPT_DISABLE
	softint_unmask(SIGALRM);
	softint_unmask(SIGUSR1);
#endif
	uv_loop_init(&uvio);
	in_callback = 0;

	while (true) {
		struct sandbox *s = sandbox_schedule_io();
		while (s) {
			sandbox_switch(s);
			s = sandbox_schedule_io();
		}
	}

	*(int *)data = -1;
	pthread_exit(data);
}

void
sandbox_run(sbox_request_t *s)
{
	// for now, a pull model...
	// sandbox_run adds to the global ready queue..
	// each sandboxing thread pulls off of that global ready queue..
	debuglog("[%p: %s]\n", s, s->mod->name);
	sandbox_deque_push(s);
}

// perhaps respond to request
void
sandbox_exit(void)
{
	struct sandbox *current_sandbox = sandbox_current();
	assert(current_sandbox);
	softint_disable();
	sandbox_local_stop(current_sandbox);
	current_sandbox->state = SANDBOX_RETURNED;
	// free resources from "main function execution", as stack still in use.
	struct sandbox *n = sandbox_schedule(0);
	assert(n != current_sandbox);
	softint_enable();
	// unmap linear memory only!
	munmap(current_sandbox->linear_start, SBOX_MAX_MEM + PAGE_SIZE);
	// sandbox_local_end(current_sandbox);
	sandbox_switch(n);
}

/**
 * @brief Execution Loop of the listener core, handles HTTP requests, allocates sandbox request objects, and pushes the sandbox object to the global dequeue
 * @param d Unknown
 * @return NULL
 * 
 * Used Globals:
 * epoll_file_descriptor - the epoll file descriptor
 * 
 */
void *
runtime_accept_thdfn(void *d)
{
	struct epoll_event *epoll_events = (struct epoll_event *)malloc(EPOLL_MAX * sizeof(struct epoll_event));
	int                 total_requests  = 0;
	while (true) {
		int ready = epoll_wait(epoll_file_descriptor, epoll_events, EPOLL_MAX, -1);
		unsigned long long int start_time_in_cycles = rdtsc();
		for (int i = 0; i < ready; i++) {
			if (epoll_events[i].events & EPOLLERR) {
				perror("epoll_wait");
				assert(0);
			}

			struct sockaddr_in client;
			socklen_t          client_len = sizeof(client);
			struct module *    m          = (struct module *)epoll_events[i].data.ptr;
			assert(m);
			int es = m->srvsock;
			int s  = accept(es, (struct sockaddr *)&client, &client_len);
			if (s < 0) {
				perror("accept");
				assert(0);
			}
			total_requests++;
			printf("Received Request %d at %lld\n", total_requests, start_time_in_cycles);

			// struct sandbox *sb = sandbox_alloc(m, m->name, s, (const struct sockaddr *)&client);
			sbox_request_t *sb = sbox_request_alloc(m, m->name, s, (const struct sockaddr *)&client, start_time_in_cycles);
			assert(sb);
		}
	}

	free(epoll_events);

	return NULL;
}

/**
 * Initialize runtime global state, mask signals, and init http server
 */
void
runtime_init(void)
{
	epoll_file_descriptor = epoll_create1(0);
	assert(epoll_file_descriptor >= 0);
	global_deque = (struct deque_sandbox *)malloc(sizeof(struct deque_sandbox));
	assert(global_deque);

	// Note: Below is a Macro
	deque_init_sandbox(global_deque, SBOX_MAX_REQS);

	softint_mask(SIGUSR1);
	softint_mask(SIGALRM);

	http_init();
}

/**
 * Initializes the listener thread, pinned to core 0, and starts to listen for requests
 */
void
runtime_thd_init(void)
{
	cpu_set_t cs;

	CPU_ZERO(&cs);
	CPU_SET(MOD_REQ_CORE, &cs);

	pthread_t iothd;
	int       ret = pthread_create(&iothd, NULL, runtime_accept_thdfn, NULL);
	assert(ret == 0);
	ret = pthread_setaffinity_np(iothd, sizeof(cpu_set_t), &cs);
	assert(ret == 0);
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cs);
	assert(ret == 0);

	softint_init();
	softint_timer_arm();
}
