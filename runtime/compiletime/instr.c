/* code from https://github.com/gwsystems/silverfish/blob/master/runtime/runtime.c */
#include <assert.h>
#include <types.h>

// TODO: Throughout here we use `assert` for error conditions, which isn't optimal
// Instead we should use `unlikely` branches to a single trapping function (which should optimize better)
// The below functions are for implementing WASM instructions

// ROTL and ROTR helper functions
INLINE u32
rotl_u32(u32 n, u32 c_u32)
{
        // WASM requires a modulus here (usually a single bitwise op, but it means we need no assert)
        unsigned int c = c_u32 % (CHAR_BIT * sizeof(n));
        const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);  // assumes width is a power of 2.

        c &= mask;
        return (n << c) | (n >> ((-c) & mask));
}

INLINE u32
rotr_u32(u32 n, u32 c_u32)
{
        // WASM requires a modulus here (usually a single bitwise op, but it means we need no assert)
        unsigned int c = c_u32 % (CHAR_BIT * sizeof(n));
        const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);

        c &= mask;
        return (n>>c) | (n << ((-c) & mask));
}

INLINE u64
rotl_u64(u64 n, u64 c_u64)
{
        // WASM requires a modulus here (usually a single bitwise op, but it means we need no assert)
        unsigned int c = c_u64 % (CHAR_BIT * sizeof(n));
        const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);  // assumes width is a power of 2.

        c &= mask;
        return (n << c) | (n >> ((-c) & mask));
}

INLINE u64
rotr_u64(u64 n, u64 c_u64)
{
        // WASM requires a modulus here (usually a single bitwise op, but it means we need no assert)
        unsigned int c = c_u64 % (CHAR_BIT * sizeof(n));
        const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);

        c &= mask;
        return (n >> c) | (n << ((-c) & mask));
}

// Now safe division and remainder
INLINE u32
u32_div(u32 a, u32 b)
{
        assert(b);
        return a / b;
}

INLINE u32
u32_rem(u32 a, u32 b)
{
        assert(b);
        return a % b;
}

INLINE i32
i32_div(i32 a, i32 b)
{
        assert(b && (a != INT32_MIN || b != -1));
        return a / b;
}

INLINE i32
i32_rem(i32 a, i32 b)
{
        assert(b && (a != INT32_MIN || b != -1));
        return a % b;
}

INLINE u64
u64_div(u64 a, u64 b)
{
        assert(b);
        return a / b;
}

INLINE u64
u64_rem(u64 a, u64 b)
{
        assert(b);
        return a % b;
}

INLINE i64
i64_div(i64 a, i64 b)
{
        assert(b && (a != INT64_MIN || b != -1));
        return a / b;
}

INLINE i64
i64_rem(i64 a, i64 b)
{
        assert(b && (a != INT64_MIN || b != -1));
        return a % b;
}

// float to integer conversion methods
// In C, float => int conversions always truncate
// If a int2float(int::min_value) <= float <= int2float(int::max_value), it must always be safe to truncate it
u32
u32_trunc_f32(float f)
{
        assert(0 <= f && f <= UINT32_MAX);
        return (u32) f;
}

i32
i32_trunc_f32(float f)
{
        assert(INT32_MIN <= f && f <= INT32_MAX );
        return (i32) f;
}

u32
u32_trunc_f64(double f)
{
        assert(0 <= f && f <= UINT32_MAX);
        return (u32) f;
}

i32
i32_trunc_f64(double f)
{
        assert(INT32_MIN <= f && f <= INT32_MAX );
        return (i32) f;
}

u64
u64_trunc_f32(float f)
{
        assert(0 <= f && f <= UINT64_MAX);
        return (u64) f;
}

i64
i64_trunc_f32(float f)
{
        assert(INT64_MIN <= f && f <= INT64_MAX);
        return (i64) f;
}

u64
u64_trunc_f64(double f)
{
        assert(0 <= f && f <= UINT64_MAX);
        return (u64) f;
}

i64
i64_trunc_f64(double f)
{
        assert(INT64_MIN <= f && f <= INT64_MAX);
        return (i64) f;
}

// Float => Float truncation functions
INLINE float
f32_trunc_f32(float f)
{
        return trunc(f);
}

INLINE float
f32_min(float a, float b)
{
        return a < b ? a : b;
}

INLINE float
f32_max(float a, float b)
{
        return a > b ? a : b;
}

INLINE float
f32_floor(float a)
{
	return floor(a);
}

INLINE double
f64_min(double a, double b)
{
        return a < b ? a : b;
}

INLINE double
f64_max(double a, double b)
{
        return a > b ? a : b;
}

INLINE double
f64_floor(double a)
{
	return floor(a);
}


