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

struct deque_sandbox *glb_dq;
pthread_mutex_t glbq_mtx = PTHREAD_MUTEX_INITIALIZER;

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
	fprintf(stderr, "(%d,%lu) %s: run %p, %s\n", sched_getcpu(), pthread_self(), __func__, s, s->mod->name);
	ps_list_head_append_d(&runq, s);
}

static inline int
sandbox_pull(void)
{
	int n = 0;

	while (n < SBOX_PULL_MAX) {
		struct sandbox *s = sandbox_deque_steal();

		if (!s) break;
		assert(s->state == SANDBOX_RUNNABLE);
		sandbox_local_run(s);
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
sandbox_schedule(void)
{
	struct sandbox *s = NULL;
	if (ps_list_head_empty(&runq)) {
		if (sandbox_pull() == 0) {
			//debuglog("[null: null]\n");
			return NULL;
		}
	}

	s = ps_list_head_first_d(&runq, struct sandbox);

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

	while (i < n) {
		i ++;
		struct sandbox *s = ps_list_head_first_d(&endq, struct sandbox);
		if (!s) break;
		ps_list_rem_d(s);
		sandbox_free(s);
	}
}

struct sandbox *
sandbox_schedule_uvio(void)
{
	sandbox_local_free(1);
	if (!in_callback) sandbox_io_nowait();

	assert(sandbox_current() == NULL);
	softint_disable();
	struct sandbox *s = sandbox_schedule();
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
	// perhaps 2 lists in the sandbox to make sure sandbox is either in runlist or waitlist..
	assert(s->state == SANDBOX_BLOCKED);
	assert(ps_list_singleton_d(s));
	s->state = SANDBOX_RUNNABLE;
	ps_list_head_append_d(&runq, s);
	softint_enable();
#endif
}

void
sandbox_block(void)
{
#ifndef STANDALONE
	// perhaps 2 lists in the sandbox to make sure sandbox is either in runlist or waitlist..
	assert(in_callback == 0);
	softint_disable();
	struct sandbox *c = sandbox_current();
	ps_list_rem_d(c);
	c->state = SANDBOX_BLOCKED;
	struct sandbox *s = sandbox_schedule();
	debuglog("[%p: %s, %p: %s]\n", c, c->mod->name, s, s ? s->mod->name: "");
	softint_enable();
	sandbox_switch(s);
#else
	uv_run(runtime_uvio(), UV_RUN_DEFAULT);
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

static inline void
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
		struct sandbox *s = sandbox_schedule_uvio();
		while (s) {
			sandbox_switch(s);
			s = sandbox_schedule_uvio();
		}
	}

	*(int *)data = -1;
	pthread_exit(data);
}

void
sandbox_run(struct sandbox *s)
{
#ifndef STANDALONE
	// for now, a pull model... 
	// sandbox_run adds to the global ready queue..
	// each sandboxing thread pulls off of that global ready queue..
	debuglog("[%p: %s]\n", s, s->mod->name);
	s->state = SANDBOX_RUNNABLE;
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

	fprintf(stderr, "(%d,%lu) %s: %p, %s exit\n", sched_getcpu(), pthread_self(), __func__, curr, curr->mod->name);
	softint_disable();
	sandbox_local_stop(curr);
	curr->state = SANDBOX_RETURNED;
	// free resources from "main function execution", as stack still in use. 
	sandbox_local_end(curr);
	sandbox_response(curr);
	struct sandbox *n = sandbox_schedule();
	softint_enable();
	sandbox_switch(n);
#else
	sandbox_switch(NULL);
#endif
}

void *
runtime_uvio_thdfn(void *d)
{
	assert(d == (void *)uv_default_loop());
	while (1) {
		// runs until there are no events..
		uv_run(uv_default_loop(), UV_RUN_DEFAULT);
		pthread_yield();
	}

	assert(0);

	return NULL;
}

void
runtime_init(void)
{
	glb_dq = (struct deque_sandbox *)malloc(sizeof(struct deque_sandbox));	
	assert(glb_dq);
	deque_init_sandbox(glb_dq, SBOX_MAX_REQS); 

	softint_mask(SIGUSR1);
	softint_mask(SIGALRM);
	cpu_set_t cs;

	CPU_ZERO(&cs);
	CPU_SET(MOD_REQ_CORE, &cs);

	pthread_t iothd;
	int ret = pthread_create(&iothd, NULL, runtime_uvio_thdfn, (void *)uv_default_loop());
	assert(ret == 0);
	ret = pthread_setaffinity_np(iothd, sizeof(cpu_set_t), &cs);
	assert(ret == 0);
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cs);
	assert(ret == 0);

	softint_init();
	softint_timer_arm();
}
