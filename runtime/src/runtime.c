#include <runtime.h>
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

struct deque_sandbox *glb_dq;
pthread_mutex_t glbq_mtx = PTHREAD_MUTEX_INITIALIZER;
int epfd;

// per-thread (per-core) run and completion queue.. (using doubly-linked-lists)
__thread static struct ps_list_head runq;
__thread static struct ps_list_head endq;

// current sandbox that is active..
__thread sandbox_t *current_sandbox = NULL;

// context pointer to switch to when this thread gets a SIGUSR1
__thread arch_context_t *next_context = NULL;

// context of the runtime thread before running sandboxes or to resume its "main".
__thread arch_context_t base_context;

// libuv i/o loop handle per sandboxing thread!
__thread uv_loop_t uvio;

static inline void
sandbox_local_run(struct sandbox *s)
{
	assert(ps_list_singleton_d(s));
//	fprintf(stderr, "(%d,%lu) %s: run %p, %s\n", sched_getcpu(), pthread_self(), __func__, s, s->mod->name);
	ps_list_head_append_d(&runq, s);
}

static inline int
sandbox_pull(void)
{
	int n = 0;

	while (n < SBOX_PULL_MAX) {
		sbox_request_t *s = sandbox_deque_steal();

		if (!s) break;
#ifndef STANDALONE
#ifdef SBOX_SCALE_ALLOC
		struct sandbox *sb = sandbox_alloc(s->mod, s->args, s->sock, s->addr);
		assert(sb);
		free(s);
		sb->state = SANDBOX_RUNNABLE;
		sandbox_local_run(sb);
#else
		assert(s->state == SANDBOX_RUNNABLE);
		sandbox_local_run(s);
#endif
#else
		assert(s->state == SANDBOX_RUNNABLE);
		sandbox_local_run(s);
#endif
		n++;
	}

	return n;
}

static __thread unsigned int in_callback;

void
sandbox_io_nowait(void)
{
#ifdef USE_UVIO
	// non-zero if more callbacks are expected
	in_callback = 1;
	int n = uv_run(runtime_uvio(), UV_RUN_NOWAIT), i = 0;
	while (n > 0) {
		n--;
		uv_run(runtime_uvio(), UV_RUN_NOWAIT);
	}
	in_callback = 0;
#endif
	// zero, so there is nothing (don't block!)
}

struct sandbox *
sandbox_schedule(int interrupt)
{
	struct sandbox *s = NULL;
	if (ps_list_head_empty(&runq)) {
		// this is in an interrupt context, don't steal work here!
		if (interrupt) return NULL;
		if (sandbox_pull() == 0) {
			//debuglog("[null: null]\n");
			return NULL;
		}
	}

	s = ps_list_head_first_d(&runq, struct sandbox);

	assert(s->state != SANDBOX_RETURNED);
	// round-robin
	ps_list_rem_d(s);
	ps_list_head_append_d(&runq, s);
	debuglog("[%p: %s]\n", s, s->mod->name);

	return s;
}

static inline void
sandbox_local_free(unsigned int n)
{
	int i = 0;

	while (i < n && !ps_list_head_empty(&endq)) {
		i++;
		struct sandbox *s = ps_list_head_first_d(&endq, struct sandbox);
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
#ifndef STANDALONE
	softint_disable();
	debuglog("[%p: %s]\n", s, s->mod->name);
	if (s->state != SANDBOX_BLOCKED) goto done;
	assert(s->state == SANDBOX_BLOCKED);
	assert(ps_list_singleton_d(s));
	s->state = SANDBOX_RUNNABLE;
	ps_list_head_append_d(&runq, s);
done:
	softint_enable();
#endif
}

void
sandbox_block(void)
{
#ifndef STANDALONE
	assert(in_callback == 0);
	softint_disable();
	struct sandbox *c = sandbox_current();
	ps_list_rem_d(c);
	c->state = SANDBOX_BLOCKED;
	struct sandbox *s = sandbox_schedule(0);
	debuglog("[%p: %s, %p: %s]\n", c, c->mod->name, s, s ? s->mod->name: "");
	softint_enable();
	sandbox_switch(s);
#else
	uv_run(runtime_uvio(), UV_RUN_DEFAULT);
#endif
}

void
sandbox_block_http(void)
{
#ifdef USE_HTTP_UVIO
#ifdef USE_HTTP_SYNC
	// realistically, we're processing all async I/O on this core when a sandbox blocks on http processing, not great!
	// if there is a way (TODO), perhaps RUN_ONCE and check if your I/O is processed, if yes, return
	// else do async block!
	uv_run(runtime_uvio(), UV_RUN_DEFAULT);
#else
	sandbox_block();
#endif
#else
	assert(0);
	//it should not be called if not using uvio for http
#endif
}

void __attribute__((noinline)) __attribute__((noreturn))
sandbox_switch_preempt(void)
{
	pthread_kill(pthread_self(), SIGUSR1);

	assert(0); // should not get here..
	while (1) ;
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
	ps_list_head_append_d(&endq, s);
}

void *
sandbox_run_func(void *data)
{
	arch_context_init(&base_context, 0, 0);

	ps_list_head_init(&runq);
	ps_list_head_init(&endq);
	softint_off = 0;
	next_context = NULL;
#ifndef PREEMPT_DISABLE
	softint_unmask(SIGALRM);
	softint_unmask(SIGUSR1);
#endif
	uv_loop_init(&uvio);	
	in_callback = 0;

	while (1) {
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
#ifndef STANDALONE
	// for now, a pull model... 
	// sandbox_run adds to the global ready queue..
	// each sandboxing thread pulls off of that global ready queue..
	debuglog("[%p: %s]\n", s, s->mod->name);
#ifndef SBOX_SCALE_ALLOC
	s->state = SANDBOX_RUNNABLE;
#endif
	sandbox_deque_push(s);
#else
	sandbox_switch(s);
#endif
}

// perhaps respond to request
void
sandbox_exit(void)
{
#ifndef STANDALONE
	struct sandbox *curr = sandbox_current();
	assert(curr);
	softint_disable();
	sandbox_local_stop(curr);
	curr->state = SANDBOX_RETURNED;
	// free resources from "main function execution", as stack still in use. 
	struct sandbox *n = sandbox_schedule(0);
	assert(n != curr);
	softint_enable();
	//sandbox_local_end(curr);
	sandbox_switch(n);
#else
	sandbox_switch(NULL);
#endif
}

void *
runtime_accept_thdfn(void *d)
{
#ifndef STANDALONE
	struct epoll_event *epevts = (struct epoll_event *)malloc(EPOLL_MAX * sizeof(struct epoll_event));
	int nreqs = 0;
	while (1) {
		int ready = epoll_wait(epfd, epevts, EPOLL_MAX, -1);
		for (int i = 0; i < ready; i++) {
			if (epevts[i].events & EPOLLERR) {
				perror("epoll_wait");
				assert(0);
			}

			struct sockaddr_in client;
			socklen_t client_len = sizeof(client);
			struct module *m = (struct module *)epevts[i].data.ptr;
			assert(m);
			int es = m->srvsock;
			int s = accept(es, (struct sockaddr *)&client, &client_len);
			if (s < 0) {
				perror("accept");
				assert(0);
			}
			nreqs++;

			//struct sandbox *sb = sandbox_alloc(m, m->name, s, (const struct sockaddr *)&client);
			sbox_request_t *sb = sbox_request_alloc(m, m->name, s, (const struct sockaddr *)&client);
			assert(sb);
		}
	}

	free(epevts);
#endif

	return NULL;
}

void
runtime_init(void)
{
	epfd = epoll_create1(0);
	assert(epfd >= 0);
	glb_dq = (struct deque_sandbox *)malloc(sizeof(struct deque_sandbox));	
	assert(glb_dq);
	deque_init_sandbox(glb_dq, SBOX_MAX_REQS); 

	softint_mask(SIGUSR1);
	softint_mask(SIGALRM);

	http_init();
}

void
runtime_thd_init(void)
{
	cpu_set_t cs;

	CPU_ZERO(&cs);
	CPU_SET(MOD_REQ_CORE, &cs);

	pthread_t iothd;
	int ret = pthread_create(&iothd, NULL, runtime_accept_thdfn, NULL);
	assert(ret == 0);
	ret = pthread_setaffinity_np(iothd, sizeof(cpu_set_t), &cs);
	assert(ret == 0);
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cs);
	assert(ret == 0);

	softint_init();
	softint_timer_arm();
}
