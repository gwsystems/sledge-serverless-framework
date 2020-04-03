/* https://github.com/gwsystems/silverfish/blob/master/runtime/libc/libc_backing.c */
#include <runtime.h>

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
	// Just do some no op
}


// Floating point routines
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

