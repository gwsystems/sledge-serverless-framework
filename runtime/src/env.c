/* https://github.com/gwsystems/silverfish/blob/master/runtime/libc/libc_backing.c */
#include <ck_pr.h>

#include "runtime.h"
#include "worker_thread.h"

extern i32 inner_syscall_handler(i32 n, i32 a, i32 b, i32 c, i32 d, i32 e, i32 f);

i32
env_syscall_handler(i32 n, i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
	i32 i = inner_syscall_handler(n, a, b, c, d, e, f);

	return i;
}

i32
env___syscall(i32 n, i32 a, i32 b, i32 c, i32 d, i32 e, i32 f)
{
	return env_syscall_handler(n, a, b, c, d, e, f);
}

void
env___unmapself(u32 base, u32 size)
{
	/* Just do some no op */
}

i32
env_a_ctz_64(u64 x)
{
	return __builtin_ctzll(x);
}

INLINE void
env_a_and_64(i32 p_off, u64 v)
{
	uint64_t *p = worker_thread_get_memory_ptr_void(p_off, sizeof(uint64_t));
	ck_pr_and_64(p, v);
}

INLINE void
env_a_or_64(i32 p_off, i64 v)
{
	assert(sizeof(i64) == sizeof(uint64_t));
	uint64_t *p = worker_thread_get_memory_ptr_void(p_off, sizeof(i64));
	ck_pr_or_64(p, v);
}

i32
env_a_cas(i32 p_off, i32 t, i32 s)
{
	assert(sizeof(i32) == sizeof(volatile int));
	int *p = worker_thread_get_memory_ptr_void(p_off, sizeof(i32));

	return ck_pr_cas_int(p, t, s);
}

void
env_a_or(i32 p_off, i32 v)
{
	assert(sizeof(i32) == sizeof(volatile int));
	int *p = worker_thread_get_memory_ptr_void(p_off, sizeof(i32));
	ck_pr_or_int(p, v);
}

i32
env_a_swap(i32 x_off, i32 v)
{
	assert(sizeof(i32) == sizeof(volatile int));
	int *x = worker_thread_get_memory_ptr_void(x_off, sizeof(i32));

	int p;
	do {
		p = ck_pr_load_int(x);
	} while (!ck_pr_cas_int(x, p, v));
	v = p;

	return v;
}

i32
env_a_fetch_add(i32 x_off, i32 v)
{
	assert(sizeof(i32) == sizeof(volatile int));
	int *x = worker_thread_get_memory_ptr_void(x_off, sizeof(i32));
	return ck_pr_faa_int(x, v);
}

void
env_a_inc(i32 x_off)
{
	assert(sizeof(i32) == sizeof(volatile int));
	int *x = worker_thread_get_memory_ptr_void(x_off, sizeof(i32));
	ck_pr_inc_int(x);
}

void
env_a_dec(i32 x_off)
{
	assert(sizeof(i32) == sizeof(volatile int));
	int *x = worker_thread_get_memory_ptr_void(x_off, sizeof(i32));
	ck_pr_dec_int(x);
}

void
env_a_store(i32 p_off, i32 x)
{
	assert(sizeof(i32) == sizeof(volatile int));
	int *p = worker_thread_get_memory_ptr_void(p_off, sizeof(i32));
	ck_pr_store_int(p, x);
}

int
env_a_ctz_32(i32 x)
{
	return __builtin_ctz(x);
}

void
env_do_spin(i32 i)
{
	ck_pr_stall();
}

void
env_do_crash(i32 i)
{
	printf("crashing: %d\n", i);
	assert(0);
}

void
env_do_barrier(i32 x)
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
