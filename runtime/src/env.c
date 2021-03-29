#include <assert.h>
#include <ck_pr.h>
#include <math.h>

#include "runtime.h"
#include "worker_thread.h"

extern int32_t inner_syscall_handler(int32_t n, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f);

int32_t
env_syscall_handler(int32_t n, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
	int32_t i = inner_syscall_handler(n, a, b, c, d, e, f);

	return i;
}

int32_t
env___syscall(int32_t n, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
	return env_syscall_handler(n, a, b, c, d, e, f);
}

void
env___unmapself(uint32_t base, uint32_t size)
{
	/* Just do some no op */
}

int32_t
env_a_ctz_64(uint64_t x)
{
	return __builtin_ctzll(x);
}

INLINE void
env_a_and_64(int32_t p_off, uint64_t v)
{
	uint64_t *p = worker_thread_get_memory_ptr_void(p_off, sizeof(uint64_t));
	ck_pr_and_64(p, v);
}

INLINE void
env_a_or_64(int32_t p_off, int64_t v)
{
	assert(sizeof(int64_t) == sizeof(uint64_t));
	uint64_t *p = worker_thread_get_memory_ptr_void(p_off, sizeof(int64_t));
	ck_pr_or_64(p, v);
}

int32_t
env_a_cas(int32_t p_off, int32_t t, int32_t s)
{
	assert(sizeof(int32_t) == sizeof(volatile int));
	int *p = worker_thread_get_memory_ptr_void(p_off, sizeof(int32_t));

	return ck_pr_cas_int(p, t, s);
}

void
env_a_or(int32_t p_off, int32_t v)
{
	assert(sizeof(int32_t) == sizeof(volatile int));
	int *p = worker_thread_get_memory_ptr_void(p_off, sizeof(int32_t));
	ck_pr_or_int(p, v);
}

int32_t
env_a_swap(int32_t x_off, int32_t v)
{
	assert(sizeof(int32_t) == sizeof(volatile int));
	int *x = worker_thread_get_memory_ptr_void(x_off, sizeof(int32_t));

	int p;
	do {
		p = ck_pr_load_int(x);
	} while (!ck_pr_cas_int(x, p, v));
	v = p;

	return v;
}

int32_t
env_a_fetch_add(int32_t x_off, int32_t v)
{
	assert(sizeof(int32_t) == sizeof(volatile int));
	int *x = worker_thread_get_memory_ptr_void(x_off, sizeof(int32_t));
	return ck_pr_faa_int(x, v);
}

void
env_a_inc(int32_t x_off)
{
	assert(sizeof(int32_t) == sizeof(volatile int));
	int *x = worker_thread_get_memory_ptr_void(x_off, sizeof(int32_t));
	ck_pr_inc_int(x);
}

void
env_a_dec(int32_t x_off)
{
	assert(sizeof(int32_t) == sizeof(volatile int));
	int *x = worker_thread_get_memory_ptr_void(x_off, sizeof(int32_t));
	ck_pr_dec_int(x);
}

void
env_a_store(int32_t p_off, int32_t x)
{
	assert(sizeof(int32_t) == sizeof(volatile int));
	int *p = worker_thread_get_memory_ptr_void(p_off, sizeof(int32_t));
	ck_pr_store_int(p, x);
}

int
env_a_ctz_32(int32_t x)
{
	return __builtin_ctz(x);
}

void
env_do_spin(int32_t i)
{
	ck_pr_stall();
}

void
env_do_crash(int32_t i)
{
	printf("crashing: %d\n", i);
	assert(0);
}

void
env_do_barrier(int32_t x)
{
	ck_pr_barrier();
}

/* Floating point routines */
INLINE double
env_sin(double d)
{
	return sin(d);
}

INLINE double
env_cos(double d)
{
	return cos(d);
}

INLINE unsigned long long
env_getcycles(void)
{
	return __getcycles();
}
